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
      transport->open();
      client.put( key, val );
      transport->close();
      }
    catch ( TTransportException e ) {
      printf( "Caught exception: %s\n", e.what());
    }
    catch ( WatRaftException e ) {
      if ( e.error_code == WatRaftErrorType::NOT_LEADER ) {
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
      transport->open();
      client.get( val, key );
      transport->close();
      }
    catch ( TTransportException e ) {
      printf( "Caught exception: %s\n", e.what());
    }
    catch ( WatRaftException e ) {
      if ( e.error_code == WatRaftErrorType::NOT_LEADER ) {
        updateServerInfo( e.node_id, config );
        return getOp( key );
      }
    }
    return val;
}



void WatRaftUser::replicationService() {
    int input;
    while ( true ) {
      std::cout << "Enter 1 to input key/val" << std::endl;
      std::cout << "Enter 2 to get value for key" << std::endl;
      std::cout << "Any other key to exit" << std::endl;
      std::cin >> input;

      std::string key;
      std::string val;
      switch( input ) {
        case (1):
                std::cout << "Key: " << std::flush;
                std::cin >> key;
                std::cout << "Value: " << std::flush;
                std::cin >> val;
                putOp( key, val );
                break;
        case (2):
                std::cout << "Key: " << std::flush;
                std::cin >> key;
                val = getOp( key );
                std::cout << val << std::endl;
                break;
        default:
                return;
      }
    }
}


} // namespace WatRaft

using namespace WatRaft;

int main( int argc, char** argv ) {
    if( argc < 3 ) {
      printf( "Usage: %s default_server config_file\n", argv[0] );
    }
    WatRaftConfig config;
    config.parse( argv[2] );
    WatRaftUser user( atoi( argv[1] ), &config );
    user.replicationService();

    return 0;
}
