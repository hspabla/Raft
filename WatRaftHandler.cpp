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
long int time() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    return ms;
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
    WatRaftState::State server_state = server->wat_state.get_state();
    gettimeofday( &(server->start ), NULL );
    static ServerData updateData;

    if ( server_state == WatRaftState::CANDIDATE && term >= currentTerm ) {
      server->wat_state.change_state( WatRaftState::FOLLOWER );
      result.term = term;
      result.success = true;
    } else if ( server_state == WatRaftState::CANDIDATE && term < currentTerm ) {
      result.term = currentTerm;
      result.success = false;
    } else if ( term >= currentTerm ) {
        if ( server_state != WatRaftState::FOLLOWER ) {
          server->wat_state.change_state( WatRaftState::FOLLOWER );
        }
        result.term = term;
        result.success = true;

    } else {
      result.term = currentTerm;
      result.success = false;
    }

    updateData.currentTerm = result.term;
    updateData.votedFor = server->serverStaticData->getData()->votedFor;
    server->serverStaticData->updateData( &updateData );

    _return = result;
}

void WatRaftHandler::request_vote(RVResult& _return,
                                  const int32_t term,
                                  const int32_t candidate_id,
                                  const int32_t last_log_index,
                                  const int32_t last_log_term) {
    RVResult result;
    int currentTerm = server->serverStaticData->getData()->currentTerm;

    std::cout << time() << ": Vote request from " << candidate_id
              << " For term : " << term
              << std::endl;

    // if our term is greater than who is asking for our vote
    if ( term < currentTerm ) {
      result.term = currentTerm;
      result.vote_granted = false;
      std::cout << time() << ": No vote, term < myTerm, my term:" << currentTerm << std::endl;
    }
    else if ( term == currentTerm ) {
      // if our term is same as guy asking for our vote, means we have already voted
      result.term = currentTerm;
      result.vote_granted = false;
      std::cout << time() << ": No vote, already voted for this term, my term: " << currentTerm << std::endl;
    }
    else {
      std::cout << time() << ": Vote granted, my term was " << currentTerm << std::endl;
      // granting vote
      struct ServerData updatedData;
      updatedData.currentTerm = term;
      updatedData.votedFor = candidate_id;
      server->serverStaticData->updateData( &updatedData );
      std::cout << time() << ": Vote granted, my term is " << updatedData.currentTerm << std::endl;
      // reset our timer
      gettimeofday( &( server->start ), NULL );

      result.term = term;
      result.vote_granted = true;
    }

    _return = result;
}

void WatRaftHandler::debug_echo(std::string& _return, const std::string& msg) {
    _return = msg;
    printf("debug_echo\n");
}
} // namespace WatRaft

