#include "WatRaftServer.h"
#include "WatRaft.h"
#include "WatRaftState.h"
#include "WatRaftHandler.h"
#include "WatRaftConfig.h"
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

int ELECTION_TIMEOUT = 10;
int MAJORITY = 2;

WatRaftServer::WatRaftServer( int node_id, const WatRaftConfig* config ) throw ( int) :
                                node_id( node_id ), rpc_server( NULL), config( config) {
    int rc = pthread_create( &rpc_thread, NULL, start_rpc_server, this );
    if ( rc != 0 ) {
        throw rc; // Just throw the error code
    }
    serverStaticData = new WatRaftStorage( node_id );
    log = serverStaticData->getLog();
    tick = clock();
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

    // change to candidate state
    wat_state.change_state( WatRaftState::CANDIDATE );

    // self vote
    int votedFor = node_id;
    quorum += 1;

    // reset election timer
    gettimeofday( &start, NULL );

    int last_log_index = getLastLogIndex();
    int last_log_term = getLastLogTerm();
    int replyTerm;

    // Send RFV message
    ServerMap::const_iterator it = config->get_servers()->begin();
    for ( ; it != config->get_servers()->end(); it++) {

      // We have received AppendRPC message and state changed to FOLLOWER
      if ( wat_state.get_state() != WatRaftState::CANDIDATE ) {
        return;
      }

      // No need to RPC ourselves
      if ( it->first == node_id ) {
        continue;
      }

      // If election timeout elapses, restart election
      gettimeofday( &current, NULL );
      double elapsedTime = ( current.tv_sec - start.tv_sec );
      if ( elapsedTime  > ( double )ELECTION_TIMEOUT ) {
        leaderElection();
        return;
      }

      // Send vote request to other server
      RVResult voteResult;
      voteResult = sendRequestVote( term,
                                    node_id,
                                    last_log_index,
                                    last_log_term,
                                    ( it->second ).ip,
                                    ( it->second ).port );
      if ( voteResult.vote_granted ) {
        quorum += 1;
        replyTerm = voteResult.term;
      }
    }
    if ( quorum > MAJORITY ) {
      // Maybe do this atomically
      struct ServerData updatedState;
      updatedState.currentTerm = replyTerm;
      updatedState.votedFor = votedFor;
      serverStaticData->updateData( &updatedState );

      wat_state.change_state( WatRaftState::LEADER );
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
    ServerMap::const_iterator it = config->get_servers()->begin();
    for ( ; it != config->get_servers()->end(); it++) {
      if ( it->first == node_id ) {
        continue; // Skip itself
      }
      AEResult helloResult;
      int myTerm = serverStaticData->getData()->currentTerm;
      std::vector<Entry> nullEntry;
      helloResult = sendAppendEntries( myTerm, node_id, 0, 0, nullEntry, commitIndex,
                                      ( it->second ).ip, (it->second).port);
      if ( helloResult.term > myTerm ) {
        wat_state.change_state( WatRaftState::FOLLOWER );
        //break;
      }
      std::cout << "Other guys term " << helloResult.term << std::endl;
    }
}

int WatRaftServer::wait() {
    wat_state.wait_ge( WatRaftState::SERVER_CREATED );
    while ( true ) {
      sleep( 1 );
      gettimeofday( &current, NULL );
      double elapsedTime = ( current.tv_sec - start.tv_sec );
      if ( elapsedTime  > (double )ELECTION_TIMEOUT ) {
        leaderElection();
      }
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
