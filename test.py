#!/usr/bin/env python
import re
import time
import sys
import pdb
from subprocess import Popen, PIPE


def start_server( serverExec, server_num, config ):
    server = Popen( [serverExec, "%d" % server_num, config], stdout=PIPE, stderr=PIPE )
    return server

def start_servers( server, config ):
    serverProcess = []
    for i in range( 1, 6 ):
        serverProcess.append( start_server( server, i, config ) )
    return serverProcess

def gather_log( processes ):
    log = []
    for process in processes:
        output = process.stdout
        log.append( output )
    return log

def stop_server( server ):
    server.kill()

def stop_servers( servers ):
    for server in servers:
        stop_server( server )

def start_client( clientProg, client_num, config, opr, key, val ):
    client = Popen( [clientProg, client_num, config, opr, key, val],
                    stdout=PIPE, stderr=PIPE )
    return client

def print_log( dev, fileOutput, regex ):
    i = 1
    for output in fileOutput:
        print "%s: %d" % ( dev, i )
        lines = output.readlines()
        for line in lines:
            match = re.search( regex, line )
            if match:
                print "\t", line,
        i += 1

if __name__ == "__main__":
    server = "./server"
    client = "./client"
    config = "servers.txt"

    regex = sys.argv[1]
    if regex == 'Election':
        servers = start_servers( server, config )
        time.sleep( 20 )
        stop_servers( servers )
        log = gather_log( servers )
        print_log( "Server", log, regex )
    elif regex == 'Replication':
        servers = start_servers( server, config )
        time.sleep( 5 )
        client1 = start_client( client, "1", config, "put", sys.argv[2], sys.argv[3] )
        time.sleep( 1 )
        client2 = start_client( client, "2", config, "get", sys.argv[2], "" )
        time.sleep( 5 )
        stop_servers( servers )
        log = gather_log( servers )
        clientLog = gather_log( [ client1, client2 ] )
        print_log( "Server", log, regex+"|(LEADER)" )
        print_log( "Client", clientLog, "Client" )

    elif regex == 'Failure':
        servers = start_servers( server, config )
        time.sleep( 5 )
        stop_server( servers[3] )
        time.sleep( 5 )
        stop_servers( servers )
        log = gather_log( servers )
        print_log( "Server", log, "Election" )




