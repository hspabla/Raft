#ifndef _WAT_RAFT_CLIENT_H_
#define _WAT_RAFT_CLIENT_H_

#include "WatRaft.h"
#include "WatRaftConfig.h"

namespace WatRaft {

class WatRaftUser {
  private:
    int server_id;
    const WatRaftConfig* config;
    std::string server_ip;
    int server_port;
    void putOp( std::string key, std::string val );
    void updateServerInfo( int node_id, const WatRaftConfig* config );

  public:
    WatRaftUser ( int node_id, const WatRaftConfig* config );
    ~WatRaftUser () {}
    void replicationService();

};

}



#endif
