# Raft
raft.github.io
Raft implementation using Apache thrift for RPC.

Server side operations :
   Leader election:
    Randomized timeouts are used for detecting leader absence. Once a node detects leader failure, that node converts itself to candidate and start new election.
   Leader :
    Process client request - read/write key/val. Replicate log upon request from user, leader replicated the key/val on majority of the servers. It keeps retrying this until replication is successful on all the servers.
    Also, it sends keepalive to other servers.

Client operation:
Clients are hardcoded to connect to any given server during initialization. If that server is not a leader, that server will reply to the client with who is the leader. Client updates this dynamically.

Client can connect to leader for following operations :
    Put - Input key/val for replication.
    Get - Get val, for give key.
