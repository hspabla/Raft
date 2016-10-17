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
    WatRaftServer(int node_id, const WatRaftConfig* config) throw (int);
    ~WatRaftServer();
    void set_rpc_server(apache::thrift::server::TThreadedServer* server);

    void sendLogUpdate();
    int getCommitIndex() { return commitIndex; }
    int getLastLogIndex();
    int getLastLogTerm();
    size_t getPrevLogIndex( Entry entry );
    int getPrevLogTerm( Entry entry );
    int get_id() { return node_id; }
    int wait();
    long int time1();
    void printLog();
    void setCommitIndex( int index ) { commitIndex = index; }
    void setMatchIndex();
    void setNextIndex();
    void updateServerTermVote( int term, int votedFor );

    WatRaftState wat_state;   // Tracks the current state of the node.
    WatRaftStorage* serverStaticData;
    bool newEntryFlag;
    int leader_id;
    std::vector<Entry> newEntries;
    struct timeval start, current;

  private:
    int node_id;
    pthread_t rpc_thread;
    static const int num_rpc_threads = 64;
    apache::thrift::server::TThreadedServer* rpc_server;

    const WatRaftConfig* config;
    int commitIndex;
    int lastApplied;
    std::map<int, int> nextIndex;
    std::map<int, int> matchIndex;

    bool checkElectionTimeout();
    int randTimeout();
    static void* start_rpc_server(void* param);
    void leaderElection();
    void sendKeepalives();
    AEResult* sendAppendEntries( int term,
                                 int node_id,
                                 int prevLogIndex,
                                 int prevLogTerm,
                                 std::vector<Entry>& entries,
                                 int leaderCommit,
                                 std::string serverIp,
                                 int serverPort);
    RVResult* sendRequestVote( int term,
                               int candidate_id,
                               int last_log_index,
                               int last_log_term,
                               std::string serverIp,
                               int serverPort );
};
} // namespace WatRaft

#endif
