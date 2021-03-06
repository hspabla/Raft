#ifndef _WAT_RAFT_STORAGE_H_
#define _WAT_RAFT_STORAGE_H_

#include "WatRaft.h"
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>


namespace WatRaft {

struct ServerData {
    int currentTerm;
    int votedFor;
};

class WatRaftStorage {
  private:
    std::string stateFile;
    std::string logFile;
    std::vector<Entry> log;
    struct ServerData serverState;

  public:
    WatRaftStorage() {}
    WatRaftStorage(int nodeId);
    ~WatRaftStorage() {}

    std::vector<Entry>* getLog(bool fromDisk=0);
    struct ServerData* getData(bool fromDisk=0);

    void updateLog(Entry* entry);
    void updateData(struct ServerData* state);
    void deleteLog( int term );
};
}






#endif
