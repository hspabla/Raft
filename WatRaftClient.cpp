#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include "WatRaftClient.h"
#include "WatRaftConfig.h"

namespace WatRaft {

WatRaftUser::WatRaftUser( int node_id, const WatRaftConfig* config ) {
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

void replicationService() {

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

    return 0;
}
