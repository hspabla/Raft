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

int MAJORITY = 3;

WatRaftServer::WatRaftServer( int node_id, const WatRaftConfig* config ) throw ( int) :
                                node_id( node_id ), rpc_server( NULL), config( config) {
    int rc = pthread_create( &rpc_thread, NULL, start_rpc_server, this );
    if ( rc != 0 ) {
        throw rc; // Just throw the error code
    }

    // intialize storage
    serverStaticData = new WatRaftStorage( node_id );
    log = serverStaticData->getLog();

    // start timer
    gettimeofday( &start, NULL );

    commitIndex = 0;
    lastApplied = 0;
}

WatRaftServer::~WatRaftServer() {
    printf( "In destructor of WatRaftServer\n" );
    delete serverStaticData;
    delete log;
    delete rpc_server;
}

int WatRaftServer::randTimeout() {
    srand ( time( NULL ) );
    // generate random number btw 300 and 450
    return rand() % 200 + 600;
}

long int time1() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return ms;
}


RVResult WatRaftServer::sendRequestVote( int term,
                                         int candidate_id,
                                         int last_log_index,
                                         int last_log_term,
                                         std::string serverIp,
                                         int serverPort ) {

    boost::shared_ptr<TSocket> socket( new TSocket( serverIp, serverPort ) );
    boost::shared_ptr<TTransport> transport( new TBufferedTransport( socket ) );
    boost::shared_ptr<TProtocol> protocol( new TBinaryProtocol( transport ) );
    WatRaftClient client( protocol );
    RVResult result;
    try {
      transport->open();
      client.request_vote( result, term,
                                   candidate_id,
                                   last_log_index,
                                   last_log_term );
      transport->close();
      }
    catch ( TTransportException e ) {
      printf( "Caught exception: %s\n", e.what());
    }
    return result;
}

void WatRaftServer::leaderElection() {
    // Quorum Size
    int quorum = 0;

    // update current term
    int term = serverStaticData->getData()->currentTerm + 1;

    // self vote
    int votedFor = node_id;
    quorum += 1;


    // change to candidate state
    wat_state.change_state( WatRaftState::CANDIDATE );

    // reset election timer
    gettimeofday( &start, NULL );

    // add getLastLogIndex and getLastLogTerm methods
    int last_log_index = 0;
    int last_log_term = 0;


    // Send RFV message
    ServerMap::const_iterator it = config->get_servers()->begin();
    for ( ; it != config->get_servers()->end(); it++) {

      // Someone else has become leader and we received append RPC message,
      // which changed our state to FOLLOWER. Need to continue this only if we
      // are still candidate.
      if ( wat_state.get_state() != WatRaftState::CANDIDATE ) {
        return;
      }

      // No need to RPC ourselves
      if ( it->first == node_id ) {
        continue;
      }

      // If election timeout elapses, restart election
      gettimeofday( &current, NULL );
      double elapsedTime = ( ( current.tv_sec * 1000 + current.tv_usec / 1000 ) -
                             ( start.tv_sec * 1000 + start.tv_usec / 1000 ) );
      if ( elapsedTime  > randTimeout() ) {
        std::cout << time1() << ": Election timeout" << std::endl;
        leaderElection();
        return;
      }

      // Send vote request to other server
      RVResult voteResult;
      std::cout << time1() << ": Sending vote to : " << it->first << std::endl;
      voteResult = sendRequestVote( term,
                                    node_id,
                                    last_log_index,
                                    last_log_term,
                                    ( it->second ).ip,
                                    ( it->second ).port );
      if ( voteResult.vote_granted ) {
        quorum += 1;
      }
      // once we have majority of votes, we
      // don't need any more votes to be a leader. This will reduce RPC messages, however
      // some servers will never vote, if we keep achieving majority before reaching them.
      if ( quorum >= MAJORITY ) {
        // update our term and voted for state
        struct ServerData updatedState;
        updatedState.currentTerm = term;
        updatedState.votedFor = votedFor;
        serverStaticData->updateData( &updatedState );
        wat_state.change_state( WatRaftState::LEADER );
        sendKeepalives();
        break;
      }
    }
    return;
}

AEResult WatRaftServer::sendAppendEntries( int term,
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
    AEResult result;
    try {
      transport->open();
      client.append_entries( result, term,
                                    node_id,
                                    prevLogIndex,
                                    prevLogTerm,
                                    entries,
                                    leaderCommit );
      transport->close();
      }
    catch ( TTransportException e ) {
      printf( "Caught exception: %s\n", e.what());
    }
    return result;
}

void WatRaftServer::sendKeepalives( ) {
    gettimeofday( &start, NULL );
    ServerMap::const_iterator it = config->get_servers()->begin();
    for ( ; it != config->get_servers()->end(); it++) {
      if ( it->first == node_id ) {
        continue; // Skip itself
      }
      //std::cout << time1() << " Sending keepalive from " << node_id
      //          << " to " << it->first << std::endl;
      AEResult helloResult;
      int myTerm = serverStaticData->getData()->currentTerm;
      std::vector<Entry> nullEntry;
      helloResult = sendAppendEntries( myTerm, node_id, 0, 0, nullEntry, commitIndex,
                                      ( it->second ).ip, (it->second).port);

      if ( helloResult.term > myTerm ) {
        wat_state.change_state( WatRaftState::FOLLOWER );
        //std::cout << time1() << ": Discoverd this node has greater term " << helloResult.term
        //          << " my term was " << myTerm
        //          << " no longer leader :( " << std::endl;
        break;
      }
    }
}

int WatRaftServer::wait() {
    wat_state.wait_ge( WatRaftState::SERVER_CREATED );
    while ( true ) {
      gettimeofday( &current, NULL );
      double elapsedTime = ( ( current.tv_sec * 1000 + current.tv_usec / 1000 ) -
                             ( start.tv_sec * 1000 + start.tv_usec / 1000 ) );
      int electionTimeout  = randTimeout();
      if ( ( wat_state.get_state() != WatRaftState::LEADER ) &&
           ( elapsedTime  > electionTimeout ) ){
        std::cout << time1() << ": elapsedTime - " << elapsedTime
                             << ": randTimeout - " << electionTimeout << std::endl;
        std::cout << time1() << ": Election timeout" << std::endl;
        leaderElection();
      }
      // sleep for 50 ms
      usleep( 200000 );
      if ( wat_state.get_state() == WatRaftState::LEADER ) {
        sendKeepalives();
      }
    }
    pthread_join( rpc_thread, NULL );
    return 0;
}

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
