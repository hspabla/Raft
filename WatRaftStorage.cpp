#include "WatRaftStorage.h"

using namespace std;

namespace WatRaft {

WatRaftStorage::WatRaftStorage(int nodeId) {
    stringstream ls, ss;
    ss << "StateServer" << nodeId;
    ls << "LogServer" << nodeId;

    stateFile = ss.str();
    logFile = ls.str();

    struct ServerData* initState;
    initState = getData(1);
    if ( !initState ) {
      struct ServerData init;
      init.currentTerm = 0;
      init.votedFor = 0;
      updateData( &init );
    } else {
      updateData( initState );
    }
    Entry initEntry;
    initEntry.term = 0;
    initEntry.key = "Key";
    initEntry.val = "Value";
    updateLog(&initEntry);

}


std::vector<Entry>* WatRaftStorage::getLog(bool fromDisk) {
    if (fromDisk) {
      fstream fs;
      fs.open(logFile.c_str(), fstream::in|ios::binary);
      vector<Entry> readLog;
      if (!fs) {
        return NULL;
      } else {
          while (!fs.eof()) {
            Entry entry;
            fs.read( reinterpret_cast<char *>(&entry), sizeof(Entry));
            readLog.push_back(entry);
          }
        }
      fs.close();
      log = readLog;
    }
    return &log;
}

void WatRaftStorage::updateLog(Entry* entry) {
    fstream fs;
    fs.open(logFile.c_str(), fstream::out|ios::binary|fstream::app);
    if (!fs) {
      cout << "Error opening file" << logFile << endl;
    } else {
      fs.write(reinterpret_cast<char *>(entry), sizeof(Entry));
    }
    fs.close();
    // updating in-memory state info
    log.push_back(*entry);
}

struct ServerData* WatRaftStorage::getData(bool fromDisk) {
    if (fromDisk) {
      struct ServerData state;
      fstream fs;
      fs.open(stateFile.c_str(), fstream::in|ios::binary);
      if (!fs) {
        return NULL;
      } else {
          fs.read(reinterpret_cast<char *>(&state), sizeof(struct ServerData));
        }
      fs.close();
      serverState = state;
    }
    return &serverState;
}


void WatRaftStorage::updateData(struct ServerData* state) {
    // updating persistent storage
    fstream fs;
    fs.open(stateFile.c_str(), fstream::out|ios::binary);

    if (!fs) {
      cout << "Error opening file" << stateFile << endl;
    } else {
      fs.write(reinterpret_cast<char *>(state), sizeof(struct ServerData));
    }
    fs.close();
    // updating in-memory state info
    serverState.currentTerm = state->currentTerm;
    serverState.votedFor = state->votedFor;
}


}
