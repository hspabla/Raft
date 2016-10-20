#include <iostream>
#include <protocol/TBinaryProtocol.h>
#include <server/TSimpleServer.h>
#include <transport/TSocket.h>
#include <transport/TServerSocket.h>
#include <transport/TBufferTransports.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PosixThreadFactory.h>
#include <thrift/server/TThreadedServer.h>

#include "WatRaft.h"
#include "WatRaftClient.h"
#include "WatRaftConfig.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;

using boost::shared_ptr;

namespace WatRaft {


WatRaftUser::WatRaftUser( int node_id, const WatRaftConfig* config ) {
    this->config = config;
    updateServerInfo( node_id, config );
}

void WatRaftUser::updateServerInfo( int node_id, const WatRaftConfig* config ){
    server_id = node_id;
    const ServerMap* servers = config->get_servers();
    ServerMap::const_iterator it = servers->find( server_id );
    server_ip = "";
    server_port = 0;
    if ( it != servers->end() ) {
      server_ip = ( it->second ).ip;
      server_port = ( it->second ).port;
    } else {
      printf ( "Invalid node_id given\n" );
    }
}

void WatRaftUser::putOp( std::string key, std::string val ) {

    boost::shared_ptr<TSocket> socket( new TSocket( server_ip, server_port ) );
    boost::shared_ptr<TTransport> transport( new TBufferedTransport( socket ) );
    boost::shared_ptr<TProtocol> protocol( new TBinaryProtocol( transport ) );
    WatRaftClient client( protocol );
    try {
      std::cout << "Client - Connecting " << server_id <<  std::endl;
      transport->open();
      client.put( key, val );
      transport->close();
      }
    catch ( TTransportException e ) {
      printf( "Caught exception: %s\n", e.what());
    }
    catch ( WatRaftException e ) {
      if ( e.error_code == WatRaftErrorType::NOT_LEADER ) {
        std::cout << "Client - Error: connecting " << server_id << ", leader is : "
                  << e.node_id << std::endl;
        updateServerInfo( e.node_id, config );
        putOp( key, val );
        return;
      }
    }
    return;
}

std::string WatRaftUser::getOp( std::string key) {

    std::string val;
    boost::shared_ptr<TSocket> socket( new TSocket( server_ip, server_port ) );
    boost::shared_ptr<TTransport> transport( new TBufferedTransport( socket ) );
    boost::shared_ptr<TProtocol> protocol( new TBinaryProtocol( transport ) );
    WatRaftClient client( protocol );
    try {
      std::cout << "Client - Connecting " << server_id <<  std::endl;
      transport->open();
      client.get( val, key );
      transport->close();
      }
    catch ( TTransportException e ) {
      printf( "Caught exception: %s\n", e.what());
    }
    catch ( WatRaftException e ) {
      if ( e.error_code == WatRaftErrorType::NOT_LEADER ) {
        std::cout << "Client - Error: connecting " << server_id << ", leader is : "
                  << e.node_id << std::endl;
        updateServerInfo( e.node_id, config );
        return getOp( key );
      } else if ( e.error_code == WatRaftErrorType::LEADER_NOT_AVAILABLE ) {
        std::cout << "Client - Error: connecting " << server_id
                  << ", no leader available" << std::endl;
        usleep( 1000 );
        return getOp( key );
      }

    }
    return val;
}



void WatRaftUser::replicationService( std::string opt, std::string key,
                                                       std::string val) {
    if ( opt == "put" ) {
        putOp( key, val );
        std::cout << "Client - put -> key : " << key << " - " << "val : " << val
                  << std::endl;
    } else if ( opt == "get" ) {
        std::string val = getOp( key );
        std::cout << "Client - get <- key : " << key << " - " << "val : " << val
                  << std::endl;
    }
}


} // namespace WatRaft

using namespace WatRaft;

int main( int argc, char** argv ) {
    if( argc < 4 ) {
      printf( "Usage: %s default_server config_file operation key val\n", argv[0] );
    }
    WatRaftConfig config;
    config.parse( argv[2] );
    WatRaftUser user( atoi( argv[1] ), &config );
    std::string opt = argv[3];
    std::string key = argv[4];
    std::string val = argv[5];
    user.replicationService( opt, key, val );

    return 0;
}
