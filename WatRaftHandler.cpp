#include "WatRaftHandler.h"
#include <string>
#include <vector>

#include "WatRaftServer.h"

namespace WatRaft {

WatRaftHandler::WatRaftHandler(WatRaftServer* raft_server) : server(raft_server) {
  // Your initialization goes here
}

WatRaftHandler::~WatRaftHandler() {}

void WatRaftHandler::get(std::string& _return, const std::string& key) {
    // Your implementation goes here
    printf("get\n");
}

void WatRaftHandler::put(const std::string& key, const std::string& val) {
    // Your implementation goes here
    printf("put\n");
}

void WatRaftHandler::append_entries(AEResult& _return,
                                    const int32_t term,
                                    const int32_t leader_id,
                                    const int32_t prev_log_index,
                                    const int32_t prev_log_term,
                                    const std::vector<Entry> & entries,
                                    const int32_t leader_commit_index) {
    AEResult result;
    int currentTerm = server->serverStaticData->getData()->currentTerm;

    // update timer
    gettimeofday( &(server->start ), NULL );

    // If we receive hello and we are leader, need to check if we should give
    // up leadership
    if ( server->wat_state.get_state() == WatRaftState::LEADER ) {
      if ( currentTerm <= term ) {
        server->wat_state.change_state( WatRaftState::FOLLOWER );
      }
    }

    result.term = currentTerm;
    result.success = true;

    std::cout << "Got keepalive " << "term " << currentTerm
              << std::endl;

    _return = result;
}

void WatRaftHandler::request_vote(RVResult& _return,
                                  const int32_t term,
                                  const int32_t candidate_id,
                                  const int32_t last_log_index,
                                  const int32_t last_log_term) {
    RVResult result;
    int currentTerm = server->serverStaticData->getData()->currentTerm;

    // if our term is greater than who is asking for our vote
    if ( term < currentTerm ) {
      result.term = currentTerm;
      result.vote_granted = false;
    }
    // if our term is same as guy asking for our vote, means we have already voted
    else if ( term == currentTerm ) {
      result.term = currentTerm;
      result.vote_granted = false;
    } else {
      int votedFor = candidate_id;
      int currentTerm = term;
      struct ServerData updatedData;
      updatedData.currentTerm = currentTerm;
      updatedData.votedFor = votedFor;
      server->serverStaticData->updateData( &updatedData );

      result.term = currentTerm;
      result.vote_granted = true;
    }

    std::cout << "Vote requested "
              << "for term " << term
              << "voted " << result.vote_granted
              << "votedFor " << server->serverStaticData->getData()->votedFor
              << std::endl;

    _return = result;
}

void WatRaftHandler::debug_echo(std::string& _return, const std::string& msg) {
    _return = msg;
    printf("debug_echo\n");
}
} // namespace WatRaft

