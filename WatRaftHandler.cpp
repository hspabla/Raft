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
    if ( server->wat_state.get_state() == WatRaftState::FOLLOWER ) {
      WatRaftException ouch;
      ouch.error_code = WatRaftErrorType::NOT_LEADER;
      ouch.error_message = "client connecting to follower";
      ouch.node_id = server->leader_id;
      throw( ouch );
    } else if ( server->wat_state.get_state() == WatRaftState::CANDIDATE ) {
      WatRaftException ouch;
      ouch.error_code = WatRaftErrorType::LEADER_NOT_AVAILABLE;
      ouch.error_message = "election going on, try later";
      throw( ouch );
    }

    // we are leader, got a client request to replicate data.
    server->setPrevLogIndex( server->getLastLogIndex() );
    server->setPrevLogTerm( server->getLastLogTerm() );

    Entry newEntry;
    newEntry.term = server->serverStaticData->getData()->currentTerm;
    newEntry.key = key;
    newEntry.val = val;
    server->serverStaticData->updateLog( &newEntry );

    server->newEntries.clear();
    server->newEntries.push_back( newEntry );
    server->newEntryFlag = true;

}

AEResult WatRaftHandler::processKeepalive( int term, int leader_id ) {
      AEResult result;
      int myTerm = server->serverStaticData->getData()->currentTerm;
      WatRaftState::State server_state = server->wat_state.get_state();
      if ( myTerm > term ) {
        result.success = false;
      } else if ( myTerm <= term ) {
          if ( server_state != WatRaftState::FOLLOWER ) {
            server->wat_state.change_state( WatRaftState::FOLLOWER );
          }
          myTerm = term;
          server->leader_id = leader_id;
          result.success = true;
      }
      result.term = myTerm;
      return result;
}
AEResult WatRaftHandler::processAppendlog( const int32_t term,
                                           const int32_t leader_id,
                                           const int32_t prev_log_index,
                                           const int32_t prev_log_term,
                                           const std::vector<Entry>* entries,
                                           const int32_t leader_commit_index ) {
    AEResult result;
    int myTerm = server->serverStaticData->getData()->currentTerm;
    if ( myTerm < term ) {
      result.success = false;
    } else if ( server->serverStaticData->getLog()->at( prev_log_index ).term !=
                prev_log_term ) {
      // we dont agree with servers logs
      result.success = false;
    } else {
      std::vector<Entry>::const_iterator it = entries->begin();
      size_t myLogIndex = prev_log_index + 1;

      // deleting corrupted log
      while ( myLogIndex > server->serverStaticData->getLog()->size() ) {
        if ( server->serverStaticData->getLog()->at( myLogIndex ).term !=
             it->term ) {
          // delete all entries from our log beyond this index
          server->serverStaticData->deleteLog( myLogIndex );
          break;
        }
        myLogIndex += 1;
        it += 1;
      }
      // updating log in persistent storage
      for ( ; it != entries->end() ; it ++ ) {
        Entry entry = *it;
        server->serverStaticData->updateLog( &entry );
      }
      server->leader_id = leader_id;

      // Updating commitIndex
      if ( leader_commit_index > server->getCommitIndex() ) {
        if ( leader_commit_index < server->getLastLogIndex() ) {
          server->setCommitIndex( leader_commit_index );
        } else {
          server->setCommitIndex( server->getLastLogIndex() );
        }
      }
      result.success = true;
    }
    result.term = server->serverStaticData->getData()->currentTerm;
    return result;
}
void WatRaftHandler::append_entries(AEResult& _return,
                                    const int32_t term,
                                    const int32_t leader_id,
                                    const int32_t prev_log_index,
                                    const int32_t prev_log_term,
                                    const std::vector<Entry> & entries,
                                    const int32_t leader_commit_index) {
    AEResult result;
    gettimeofday( &(server->start ), NULL );
    if ( entries.size() == 0 ) {
      result = processKeepalive( term, leader_id );
    } else {
      result = processAppendlog( term, leader_id, prev_log_index, prev_log_term, &entries,
                                 leader_commit_index );
    }
    server->updateServerTermVote( result.term,
                                  server->serverStaticData->getData()->votedFor );
    _return = result;
}

void WatRaftHandler::request_vote( RVResult& _return,
                                   const int32_t term,
                                   const int32_t candidate_id,
                                   const int32_t last_log_index,
                                   const int32_t last_log_term ) {
    RVResult result;
    int currentTerm = server->serverStaticData->getData()->currentTerm;

    std::cout << server->time1() << ": Vote request from " << candidate_id
              << " For term : " << term
              << std::endl;

    // if our term is greater than who is asking for our vote
    if ( term < currentTerm ) {
      result.vote_granted = false;
      std::cout << server->time1() << ": No vote, vote requested for term < myTerm,\
                                      my term: " << currentTerm << std::endl;
    }
    else if ( term == currentTerm ) {
      // if our term is same as guy asking for our vote, means we have already voted
      result.vote_granted = false;
      std::cout << server->time1() << ": No vote, already voted for this term,\
                                     my term: " << currentTerm << std::endl;
    }
    else if ( server->getLastLogTerm() > last_log_term ) {
      std::cout << server->time1()
                << ": No Vote, my last log term > request vote term index "
                << server->getLastLogTerm() << " > " << last_log_term << std::endl;
      result.vote_granted = false;
    }
    else if ( server->getLastLogTerm() == last_log_term ) {
      if ( server->getLastLogIndex() > last_log_index ) {
        std::cout << server->time1()
                  << ": No Vote, my log > request vote log "
                  << server->getLastLogIndex() << " < "
                  << last_log_index << std::endl;
        result.vote_granted = false;
      } else {
        server->updateServerTermVote( term, candidate_id );
        gettimeofday( &( server->start ), NULL );
        result.vote_granted = true;
      }
    }
    else {
      server->updateServerTermVote( term, candidate_id );
      gettimeofday( &( server->start ), NULL );
      result.vote_granted = true;
    }
    result.term = server->serverStaticData->getData()->currentTerm;
    _return = result;
}

void WatRaftHandler::debug_echo(std::string& _return, const std::string& msg) {
    _return = msg;
    printf("debug_echo\n");
}
} // namespace WatRaft

