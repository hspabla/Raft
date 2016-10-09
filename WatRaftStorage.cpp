#include "WatRaftStorage.h"

using namespace std;

namespace WatRaft {

WatRaftStorage::WatRaftStorage(int nodeId) {
    stringstream ls, ss;
    ss << "StateServer" << nodeId;
    ls << "LogServer" << nodeId;

    stateFile = ss.str();
    logFile = ls.str();

    struct ServerData initState;
    initState.currentTerm = 0;
    initState.votedFor = 0;
    updateState(&initState);

    Entry initEntry;
    initEntry.term = 0;
    initEntry.key = "Key";
    initEntry.val = "Value";
    updateLog(initEntry);

}


vector<Entry>* WatRaftStorage::getLog() {
    fstream fs;
    fs.open(logFile.c_str(), fstream::in|ios::binary);
    vector<Entry>* pLog;
    pLog = &log;
    if (!fs) {
      return NULL;

    } else {
        string line;
        while (getline(fs, line)) {
          stringstream ss(line);
          struct Entry entry;
          ss >> entry.term >> entry.key >> entry.val;
          if (ss) {
            pLog->push_back(entry);
          } else {
            cerr << "Error parsing: " << line << endl;
          }
        }
      }
    fs.close();
    return pLog;
}

struct ServerData* WatRaftStorage::getState(bool fromDisk) {
    if (fromDisk) {
      struct ServerData state;
      fstream fs;
      fs.open(stateFile.c_str(), fstream::in|ios::binary);
      if (!fs) {
        cerr << "Error opening file " << stateFile << endl;
        return NULL;
      } else {
          fs.read(reinterpret_cast<char *>(&state), sizeof(struct ServerData));
        }
      fs.close();
      serverState = state;
    }
    return &serverState;
}


void WatRaftStorage::updateState(struct ServerData* state) {
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


void WatRaftStorage::updateLog(Entry entry) {
    fstream fs;
    fs.open(logFile.c_str(), fstream::out|fstream::app);
    if (!fs) {
      cerr << "Error opening file" << logFile << endl;
    } else {
      fs << entry.term << entry.key << entry.val << '\n';
    }
}
}
