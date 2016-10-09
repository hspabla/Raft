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

    std::vector<Entry>* getLog();
    struct ServerData* getState(bool fromDisk = 0);

    void updateLog(Entry entry);
    void updateState(struct ServerData* state);
};
}






#endif
