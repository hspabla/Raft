#ifndef _WAT_RAFT_SERVER_H_
#define _WAT_RAFT_SERVER_H_

#include "WatRaft.h"
#include "WatRaftState.h"
#include "WatRaftStorage.h"
#include <ctime>
#include <pthread.h>
#include <string>
#include <thrift/server/TThreadedServer.h>
#include <vector>
#include <sys/time.h>
#define DEBUG
namespace WatRaft {

class WatRaftConfig; // Forward declaration
class WatRaftServer {
  public:
    WatRaftStorage* serverStaticData;
    std::vector<Entry>* log;
    struct timeval start, current;
    WatRaftState wat_state;   // Tracks the current state of the node.

    WatRaftServer(int node_id, const WatRaftConfig* config) throw (int);
    ~WatRaftServer();
    int wait();
    void set_rpc_server(apache::thrift::server::TThreadedServer* server);
    int get_id() { return node_id; }
    void updateServerTermVote( int term, int votedFor );
    long int time1();
    void printLog( std::vector<Entry>* log );

  private:
    int node_id;
    apache::thrift::server::TThreadedServer* rpc_server;
    const WatRaftConfig* config;
    pthread_t rpc_thread;
    static const int num_rpc_threads = 64;
    static void* start_rpc_server(void* param);

    int commitIndex;
    int lastApplied;
    std::vector<int> nextIndex;
    std::vector<int> matchIndex;

    int randTimeout();
    bool checkElectionTimeout();
    void sendKeepalives();
    void leaderElection();
    int getPrevLogIndex();
    int getPrevLogTerm();
    int getLastLogIndex();
    int getLastLogTerm();
    AEResult sendAppendEntries(int term,
                               int node_id,
                               int prevLogIndex,
                               int prevLogTerm,
                               std::vector<Entry>& entries,
                               int leaderCommit,
                               std::string serverIp,
                               int serverPort);
    RVResult sendRequestVote( int term,
                              int candidate_id,
                              int last_log_index,
                              int last_log_term,
                              std::string serverIp,
                              int serverPort );

};
} // namespace WatRaft

#endif
