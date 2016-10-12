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

namespace WatRaft {

extern int ELECTION_TIMEOUT;
extern int MAJORITY;

class WatRaftConfig; // Forward declaration
class WatRaftServer {
  public:
    WatRaftServer(int node_id, const WatRaftConfig* config) throw (int);
    ~WatRaftServer();

    // Block and wait until the server shutdowns.
    int wait();
    // Set the RPC server once it is created in a child thread.
    void set_rpc_server(apache::thrift::server::TThreadedServer* server);
    int get_id() { return node_id; }

    // Persistent state
    WatRaftStorage* serverStaticData;
    std::vector<Entry>* log;

    // timer
    struct timeval start, current;
    int randTimeout();
    
    WatRaftState wat_state;   // Tracks the current state of the node.

  private:
    int node_id;
    apache::thrift::server::TThreadedServer* rpc_server;
    const WatRaftConfig* config;
    pthread_t rpc_thread;
    static const int num_rpc_threads = 64;
    static void* start_rpc_server(void* param);

    // Raft data


    // Volatile state
    int commitIndex;
    int lastApplied;

    // Only for leader
    std::vector<int> nextIndex;
    std::vector<int> matchIndex;

    // index of log entry immediately preceding new ones
    int getPrevLogIndex();

    // term of prevLogIndex
    int getPrevLogTerm();

    // index of last log entry
    int getLastLogIndex();

    // term of last log entry
    int getLastLogTerm();

    void sendKeepalives();
    AEResult sendAppendEntries(int term,
                               int node_id,
                               int prevLogIndex,
                               int prevLogTerm,
                               std::vector<Entry>& entries,
                               int leaderCommit,
                               std::string serverIp,
                               int serverPort);
    void leaderElection();
    RVResult sendRequestVote( int term,
                              int candidate_id,
                              int last_log_index,
                              int last_log_term,
                              std::string serverIp,
                              int serverPort );

};
} // namespace WatRaft

#endif
