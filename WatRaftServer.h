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
//#define DEBUG

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
    void printLog();
    int leader_id;

    int currentLogIndex() { return log->size(); }
    int currentLogTerm() { return (*log)[ currentLogIndex() ].term; }

    int getPrevLogIndex() { return prevLogIndex; }
    int getPrevLogTerm() { return prevLogTerm; }
    int getLastLogIndex() { return lastLogIndex; }
    int getLastLogTerm() { return lastLogTerm; }
    int getCommitIndex() { return commitIndex; }
    void setPrevLogIndex( int index ) { prevLogIndex = index; }
    void setPrevLogTerm( int term ) { prevLogTerm = term; }
    void setLastLogIndex( int index ) { lastLogIndex = index; }
    void setLastLogTerm( int term ) { lastLogTerm = term; }
    void setCommitIndex( int index ) { commitIndex = index; }

    bool sendLogUpdate( std::vector<Entry>& newEntries );
  private:
    int node_id;
    apache::thrift::server::TThreadedServer* rpc_server;
    const WatRaftConfig* config;
    pthread_t rpc_thread;
    static const int num_rpc_threads = 64;
    static void* start_rpc_server(void* param);

    int commitIndex;
    int lastApplied;
    int prevLogIndex;
    int prevLogTerm;
    int lastLogIndex;
    int lastLogTerm;

    std::vector<int> nextIndex;
    std::vector<int> matchIndex;

    int randTimeout();
    bool checkElectionTimeout();
    void sendKeepalives();
    void leaderElection();


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
