CXX = g++
CPPFLAGS = -g -fpermissive -Wall -I. -I${HOME}/project/include -I${HOME}/project/include/thrift -Igen-cpp
LDFLAGS = -L${HOME}/project/lib -lthrift -lpthread
LD = g++

PROGRAMS = server client

SERVER_OBJECTS = WatRaftServer.o WatRaftHandler.o WatRaftState.o WatRaftConfig.o WatRaftStorage.o \
								 gen-cpp/WatRaft_constants.o gen-cpp/WatRaft.o gen-cpp/WatRaft_types.o

SERVER_INCFILES = WatRaftHandler.h WatRaftServer.h WatRaftState.h WatRaftConfig.h WatRaftStorage.h \
									gen-cpp/WatRaft_constants.h gen-cpp/WatRaft.h gen-cpp/WatRaft_types.h

CLIENT_OBJECTS = WatRaftClient.o WatRaftConfig.o
CLIENT_INCFILES = WatRaftClient.h WatRaftConfig.h

all: $(PROGRAMS) $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(SERVER_INCFILES) $(CLIENT_INCFILES)

server: $(SERVER_OBJECTS)
	$(LD) $^ $(LDFLAGS) -o $@

client: $(CLIENT_OBJECTS)
	$(LD) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o $(PROGRAMS) *~ LogServer* StateServer*
