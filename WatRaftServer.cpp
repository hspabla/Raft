#include "WatRaftServer.h"
#include "WatRaft.h"
#include "WatRaftState.h"
#include "WatRaftHandler.h"
#include "WatRaftConfig.h"
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <transport/TSocket.h>
#include <transport/TServerSocket.h>
#include <transport/TBufferTransports.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/server/TThreadedServer.h>


using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;

using boost::shared_ptr;

namespace WatRaft {


WatRaftServer::WatRaftServer( int node_id, const WatRaftConfig* config )
                              throw ( int ) : node_id( node_id ), rpc_server( NULL),
                                              config( config ) {

    int rc = pthread_create( &rpc_thread, NULL, start_rpc_server, this );
    if ( rc != 0 ) {
        throw rc; // Just throw the error code
    }

    // intialize storage
    serverStaticData = new WatRaftStorage( node_id );

    // start timer
    gettimeofday( &start, NULL );

    // initialize raft state
    lastApplied = 0;
    newEntryFlag = false;
    setCommitIndex( 0 );

}

WatRaftServer::~WatRaftServer() {
    printf( "In destructor of WatRaftServer\n" );
    delete serverStaticData;
    delete rpc_server;
}

// ----------------------------- HELPER FUNCTIONS --------------------------------//

int WatRaftServer::getLastLogIndex() {
    return serverStaticData->getLog()->size() - 1;
}

int WatRaftServer::getLastLogTerm() {
    return serverStaticData->getLog()->back().term;
}

void WatRaftServer::printLog() {
    std::cout << "--------- Entries in Log --------" << std::endl;
    std::vector<Entry>::iterator it = serverStaticData->getLog()->begin();
    for ( ; it != serverStaticData->getLog()->end() ; it++ ) {
      std::cout << "Term: " << it->term
                << " Key: " << it->key
                << " Value: " << it->val << std::endl;
    }
    std::cout << "---------------------------------" << std::endl;
}

int WatRaftServer::randTimeout() {
/* generate random number btw 600 and 800 */
    srand ( time( NULL ) );
    return rand() % 200 + 600;
}

long int WatRaftServer::time1() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return ms;
}

void WatRaftServer::updateServerTermVote( int term, int votedFor ) {
    static ServerData updateData;
    updateData.currentTerm = term;
    updateData.votedFor = votedFor;
    serverStaticData->updateData( &updateData );
}

void WatRaftServer::setNextIndex() {
    ServerMap::const_iterator it = config->get_servers()->begin();
    for( ; it != config->get_servers()->end(); it++ ) {
      if ( it->first == node_id ) {
        continue;
      }
      nextIndex[ it->first ] = getLastLogIndex() + 1;
    }
}

void WatRaftServer::setMatchIndex() {
    ServerMap::const_iterator it = config->get_servers()->begin();
    for( ; it != config->get_servers()->end(); it++ ) {
      if ( it->first == node_id ) {
        continue;
      }
      matchIndex[ it->first ] = 0;
    }
}

size_t WatRaftServer::getPrevLogIndex( Entry entry ){
    size_t index = find( serverStaticData->getLog()->begin(),
                      serverStaticData->getLog()->end(), entry ) -
                                                serverStaticData->getLog()->begin();
    if ( index >= serverStaticData->getLog()->size() ) {
      return 0;
    }
    return index - 1;
}

int WatRaftServer::getPrevLogTerm( Entry entry ){
    size_t index = getPrevLogIndex( entry );
    return serverStaticData->getLog()->at( index ).term;
}

bool WatRaftServer::checkElectionTimeout() {
/*  Randomaly chose election timeout period. If we have not received keepalive in
 *  that period ( which will happen in another thread ) election timer has expired
 *  notify the caller.
 */
      bool electionTime = false;
      gettimeofday( &current, NULL );
      double elapsedTime = ( ( current.tv_sec * 1000 + current.tv_usec / 1000 ) -
                             ( start.tv_sec * 1000 + start.tv_usec / 1000 ) );
      int electionTimeout  = randTimeout();
      if ( ( wat_state.get_state() != WatRaftState::LEADER ) &&
           ( elapsedTime  > electionTimeout ) ){
        std::cout << time1() << ": elapsedTime - " << elapsedTime
                             << ": randTimeout - " << electionTimeout << std::endl;
        std::cout << time1() << ": Election timeout" << std::endl;
        electionTime = true;
      }
      return electionTime;
}

void WatRaftServer::sendKeepalives( ) {
/* Leader sends a keepalive, but first reset our timer.
 * appendEntry with empty log entry is keepalive message
 * TODO: make log entry NULL
 * TODO: What should be prevLogIndex and prevLogTerm, do we need to update them in
 *       keepalive message ?
 */
    gettimeofday( &start, NULL );
    ServerMap::const_iterator it = config->get_servers()->begin();
    for ( ; it != config->get_servers()->end(); it++) {
      if ( it->first == node_id ) {
        continue;
      }
      #ifdef DEBUG
      std::cout << time1() << " Sending keepalive from " << node_id
                << " to " << it->first << std::endl;
      #endif
      AEResult* helloResult;
      int myTerm = serverStaticData->getData()->currentTerm;
      std::vector<Entry> nullEntry;
      helloResult = sendAppendEntries( myTerm, node_id, 0, 0, nullEntry, commitIndex,
                                      ( it->second ).ip, (it->second).port);

      if ( helloResult ) {
        if ( helloResult->term > myTerm ) {
          wat_state.change_state( WatRaftState::FOLLOWER );
          updateServerTermVote( helloResult->term,
                              serverStaticData->getData()->votedFor );
          #ifdef DEBUG
          std::cout << time1() << ": Discoverd node with greater term "
                  << helloResult->term
                  << " my term was " << myTerm
                  << " no longer leader :( " << std::endl;
          #endif
          delete helloResult;
          break;
        }
        delete helloResult;
      }
    }
}

void WatRaftServer::sendLogUpdate() {
    int term = serverStaticData->getData()->currentTerm;
    int quorum = 1;                               // since we already have the entry
    ServerMap::const_iterator it = config->get_servers()->begin();

    for ( ; it != config->get_servers()->end(); it++) {
      if ( it->first == node_id ) {
        continue;
      }
      //  last log index >= nextIndex for a follower, send log from nextIndex

      if ( getLastLogIndex() >= nextIndex[ it->first ] ) {
        std::vector<Entry> newEntries;
        std::vector<Entry>::iterator i = find( serverStaticData->getLog()->begin(),
                                               serverStaticData->getLog()->end(),
                                               (*(serverStaticData->getLog()))
                                                        [ nextIndex[ it->first ] ] );
        for( ; i != serverStaticData->getLog()->end(); i++ ) {
          newEntries.push_back( *i );
        }
        Entry firstEntry = newEntries[ 0 ];
        int prevLogIndex = getPrevLogIndex( firstEntry );
        int prevLogTerm = getPrevLogTerm( firstEntry );

        AEResult* appendResult;
        appendResult = sendAppendEntries( term, node_id, prevLogIndex, prevLogTerm,
                                        newEntries, commitIndex,
                                        ( it->second ).ip, ( it->second ).port );
        if ( appendResult ) {
          if ( appendResult->success ) {
            quorum += 1;
            nextIndex[ it->first ] = nextIndex[ it->first ] + newEntries.size();
            matchIndex[ it->first ] = getLastLogIndex();
          }
          else {
            nextIndex[ it->first ] = nextIndex[ it->first ] - 1;
          }
          delete appendResult;
        }
        if ( getLastLogIndex() > commitIndex ) {
          if ( quorum >= config->get_majority() && getLastLogTerm() == term ) {
            commitIndex = getLastLogIndex();
          }
        }
      }
    }
}


// -------------------------- RFV and AE Messages --------------------------------//

RVResult* WatRaftServer::sendRequestVote( int term,
                                         int candidate_id,
                                         int last_log_index,
                                         int last_log_term,
                                         std::string serverIp,
                                         int serverPort ) {

    boost::shared_ptr<TSocket> socket( new TSocket( serverIp, serverPort ) );
    boost::shared_ptr<TTransport> transport( new TBufferedTransport( socket ) );
    boost::shared_ptr<TProtocol> protocol( new TBinaryProtocol( transport ) );
    WatRaftClient client( protocol );
    RVResult* result = new RVResult();
    try {
      transport->open();
      client.request_vote( *result, term,
                                   candidate_id,
                                   last_log_index,
                                   last_log_term );
      transport->close();
      }
    catch ( TTransportException e ) {
      #ifdef DEBUG
      printf( "Caught exception: %s\n", e.what());
      #endif
      delete result;
      return NULL;
    }
    return result;
}

AEResult* WatRaftServer::sendAppendEntries( int term,
                                          int node_id,
                                          int prevLogIndex,
                                          int prevLogTerm,
                                          std::vector<Entry>& entries,
                                          int leaderCommit,
                                          std::string serverIp,
                                          int serverPort ) {

    boost::shared_ptr<TSocket> socket( new TSocket( serverIp, serverPort ));
    boost::shared_ptr<TTransport> transport( new TBufferedTransport( socket ));
    boost::shared_ptr<TProtocol> protocol( new TBinaryProtocol( transport ));
    WatRaftClient client( protocol );
    AEResult* result = new AEResult();
    try {
      transport->open();
      client.append_entries( *result, term,
                                    node_id,
                                    prevLogIndex,
                                    prevLogTerm,
                                    entries,
                                    leaderCommit );
      transport->close();
      }
    catch ( TTransportException e ) {
    #ifdef DEBUG
      printf( "Caught exception: %s\n", e.what());
    #endif
      delete result;
      return NULL;
    }
    return result;
}

// -------------------------- LEADER ELECTION ------------------------------------//

void WatRaftServer::leaderElection() {
/* Leader election protocol
 * We need majority quorum to win an election.
 * Order of operation :
 *  On conversion to candidate, start election:
 *    • Increment currentTerm
 *    • Vote for self
 *    • Reset election timer
 *    • Send RequestVote RPCs to all other servers
 *  • If votes received from majority of servers: become leader
 *  • If AppendEntries RPC received from new leader: convert to follower
 *  • If election timeout elapses: start new election
 */
    int quorum = 0;
    wat_state.change_state( WatRaftState::CANDIDATE );
    int term = serverStaticData->getData()->currentTerm + 1;
    int votedFor = node_id;
    quorum += 1;
    gettimeofday( &start, NULL );

    int last_log_index = getLastLogIndex();
    int last_log_term = getLastLogTerm();

    std::cout << time1() << ": Starting new election for term : "
                         << term << std::endl;

    ServerMap::const_iterator it = config->get_servers()->begin();
    for ( ; it != config->get_servers()->end(); it++) {

      // Someone else has become leader and we received append RPC message,
      // which changed our state to FOLLOWER.
      if ( wat_state.get_state() != WatRaftState::CANDIDATE ) {
        return;
      }

      if ( it->first == node_id ) {
        continue;
      }

      if ( checkElectionTimeout() ) {
        leaderElection();
        return;
      }

      // Send vote request to other server
      RVResult* voteResult;
      std::cout << time1() << ": Requesting vote from : " << it->first << std::endl;
      voteResult = sendRequestVote( term,
                                    node_id,
                                    last_log_index,
                                    last_log_term,
                                    ( it->second ).ip,
                                    ( it->second ).port );
      if ( voteResult ) {
        if ( voteResult->term > term ) {
          // more upto date server is present
          wat_state.change_state( WatRaftState::FOLLOWER );
          updateServerTermVote( voteResult->term,
                                serverStaticData->getData()->votedFor );
          delete voteResult;
          break;
        }
        if ( voteResult->vote_granted ) {
          quorum += 1;
        }

        // once we have majority of votes, we don't need any more votes to be a leader.
        // This will reduce RPC messages, however some servers will never vote, if we
        // keep achieving majority before reaching them. I dont think its a problem
        // keepalive will notify them.
        if ( quorum >= config->get_majority() ) {
          updateServerTermVote( term, votedFor );
          wat_state.change_state( WatRaftState::LEADER );
          leader_id = get_id();
          setNextIndex();
          setMatchIndex();
          sendKeepalives();
          delete voteResult;
          break;
        }
        delete voteResult;
      }
    }
    return;
}


// ------------------------- MAIN ACTIVITY LOOP ----------------------------------//

int WatRaftServer::wait() {
    wat_state.wait_ge( WatRaftState::SERVER_CREATED );
    while ( true ) {
      if ( wat_state.get_state() != WatRaftState::LEADER &&
           checkElectionTimeout() ) {
        leaderElection();
      }
      // sleep for 50 ms
      usleep( 50000 );
      if ( wat_state.get_state() == WatRaftState::LEADER ) {
        sendKeepalives();
        sendLogUpdate();
      }
    }
    pthread_join( rpc_thread, NULL );
    return 0;
}

// -------------------------- SERVER THREAD --------------------------------------//

void WatRaftServer::set_rpc_server( TThreadedServer* server ) {
    rpc_server = server;
    wat_state.change_state( WatRaftState::SERVER_CREATED );
}

void* WatRaftServer::start_rpc_server( void* param ) {
    WatRaftServer* raft = static_cast<WatRaftServer*>( param );
    shared_ptr<WatRaftHandler> handler( new WatRaftHandler(raft ));
    shared_ptr<TProcessor> processor( new WatRaftProcessor(handler ));
    // Get IP/port for this node
    IPPortPair this_node =
        raft->config->get_servers()->find(raft->node_id)->second;
    shared_ptr<TServerTransport> serverTransport(
        new TServerSocket( this_node.ip, this_node.port ));
    shared_ptr<TTransportFactory> transportFactory(
        new TBufferedTransportFactory());
    shared_ptr<TProtocolFactory> protocolFactory( new TBinaryProtocolFactory());
    shared_ptr<ThreadManager> threadManager =
        ThreadManager::newSimpleThreadManager( num_rpc_threads, 0 );
    shared_ptr<PosixThreadFactory> threadFactory =
        shared_ptr<PosixThreadFactory>( new PosixThreadFactory());
    threadManager->threadFactory( threadFactory );
    threadManager->start();
    TThreadedServer* server = new TThreadedServer(
        processor, serverTransport, transportFactory, protocolFactory );
    raft->set_rpc_server( server );
    server->serve();
    return NULL;
}

} // namespace WatRaft

using namespace WatRaft;

int main( int argc, char **argv ) {
    if ( argc < 3 ) {
        printf( "Usage: %s server_id config_file\n", argv[0] );
        return -1;
    }
    WatRaftConfig config;
    config.parse( argv[2] );
    try {
        WatRaftServer server( atoi(argv[1] ), &config);
        server.wait(); // Wait until server shutdown.
    } catch ( int rc ) {
        printf( "Caught exception %d, exiting\n", rc );
        return -1;
    }
    return 0;
}
