#!/usr/bin/env python3

import sys  # For arg parsing
import socket  # For sockets
import math
import select
# reachable = dict()  # reachable dictionary
xPOS = 0
yPOS = 0
cliNAME = ''
cliRANGE = 0
server_socket = None
def dist(x1, x2, y1, y2):
    return math.sqrt((int(x2)-int(x1))**2 + (int(y2)-int(y1))**2)

def sortedByDist(reachable, dest):
    """
    @requres reachable is a list of all in range sensors/base stations
    @param reachable is a list of (id, x, y)
    @param dest is the destination base station or sensor (x, y)
    @returns a list of the reachable sorted by their distance to the dest
            breaking ties using their id's in lexical order
    """
    destX, destY = dest
    # Sorts by distance then lexical. Then only return the reachable list that was sorted.
    return [n[-1] for n in sorted(reachable, key=lambda node: (dist(node[1], destX, node[2], destY), node[0], node))]

def UPDATEPOSITION(x, y):
    """
    @param x is the new x position
    @param y is the new y position
    @returns the dictionary of reachable nodes returned from the control
             nodeID => (x, y)
    """
    # Proceeds to update the position of the sensor
    send_string = "UPDATEPOSITION {} {} {} {}".format(cliNAME, cliRANGE, x, y)
    server_socket.send(send_string.encode('utf-8'))

    recv_string = server_socket.recv(1024).decode('utf-8')
    reach = recv_string.split()[2:]
    reachable = {}
    for i in range(0, int(len(reach)/3)):
        # stores the reachable sensors and base stations (x,y)
        ind = i * 3
        reachable[reach[ind]] = (reach[ind+1], reach[ind+2])
    return reachable

def WHERE(currID):
    """
    @param ID is the id of the node you want the location of
    @returns (ID, x, y) of the node you're interested
    """
    send_string = "WHERE {}".format(currID)
    server_socket.send(send_string.encode('utf-8'))
    recv_string = server_socket.recv(1024)
    data = recv_string.decode('utf-8').split()
    # print(data)
    return (data[1], int(data[2]), int (data[3]))

def run_client():
    global server_socket
    global xPOS
    global yPOS
    global cliNAME
    global cliRANGE
    if len(sys.argv) != 7:
        print(
            f"Proper usage is {sys.argv[0]} [control address] [control port] [SensorID] [SensorRange] [InitalXPosition] [InitialYPosition]")
        sys.exit(0)

    # Create the TCP socket, connect to the server
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # bind takes a 2-tuple, not 2 arguments
    # Defualt to local network
    # if sys.argv[1] == 'control':
    #     server_socket.connect(('127.0.0.1', int(sys.argv[2])))
    # else:
    #     server_socket.connect((int(sys.argv[1]), int(sys.argv[2])))
    server_socket.connect((socket.gethostbyname(sys.argv[1]), int(sys.argv[2])))

    #initial update
    xPOS = int(sys.argv[5])
    yPOS = int(sys.argv[6])
    cliNAME = sys.argv[3]
    cliRANGE = int(sys.argv[4])

    UPDATEPOSITION(xPOS, yPOS)

    while True:
        rlist = select.select([sys.stdin, server_socket], [], [])[0]
        if sys.stdin in rlist:
            # Read a string from standard input
            send_string = sys.stdin.readline()
            cmd = send_string.split()
            if not send_string or send_string == 'QUIT':
                # Disconnect from the server
                print("Closing connection to server")
                server_socket.close()
                break
            elif cmd[0] == 'WHERE':
                WHERE(cmd[1])
            elif cmd[0] == 'MOVE':
                tmp = send_string.split()
                xPOS = int(tmp[1])
                yPOS = int(tmp[2])
                UPDATEPOSITION(xPOS, yPOS)
            elif cmd[0] == 'SENDDATA':  # SENDDATA [DestinationID]
                #get an update version of the reachable list
                reachable = UPDATEPOSITION(xPOS, yPOS)
                if len(reachable) == 0:
                    send_string = print('{}: Message from {} to {} could not be delivered.'.format(cliNAME, cliNAME, cmd[1]))
                else:
                    destID, destX, destY = WHERE(cmd[1]) 
                    # print('{} {} {}'.format(destID,destX,destY))
                    reach_sort = [(dist(reachable[key][0], destX , reachable[key][1], destY), key) for key in reachable]
                    reach_sort = sorted(reach_sort)
                    nextID = reach_sort[0][1]
                    # print(reach_sort)
                    send_string = 'DATAMESSAGE {} {} {} {} {}'.format(cliNAME, nextID, destID, 1, ' '.join([cliNAME]))
                    server_socket.sendall(send_string.encode('utf-8'))

                    if nextID == destID:
                        print(f"{cliNAME}: Sent a new message directly to {destID}.", flush=True)
                    else:
                        print(f"{cliNAME}: Sent a new message bound for {destID}.", flush=True)


        if server_socket in rlist:
            # We got DATAMESSAGE
            msg = server_socket.recv(1024).decode("utf-8").split()
            # print(msg)
            originID, nextID, destID = msg[1:4]
            if destID == cliNAME:
                print("{}: Message from {} to {} successfully received.".format(cliNAME, originID, destID))
            else:
                reachable = UPDATEPOSITION(xPOS, yPOS)
                destID, destX, destY = WHERE(destID)
                reach_sort = [(dist(reachable[key][0], xPOS, reachable[key][1], yPOS), key) for key in reachable if key not in msg[5:]]
                reach_sort = sorted(reach_sort)
                # print(reach_sort)
                if len(reach_sort) == 0:
                    print('{}: Message from {} to {} could not be delivered.'.format(cliNAME, originID, destID))
                else:
                    for i in reach_sort:
                        if i[1] == destID:
                            reach_sort[0] = i
                    print('{}: Message from {} to {} being forwarded through {}'.format(cliNAME, originID, destID, cliNAME))
                    hop = msg[5:]
                    hop.append(reach_sort[0][1])
                    send_string = 'DATAMESSAGE {} {} {} {} {}'.format(
                        originID, reach_sort[0][1], destID, int(msg[4])+1, " ".join(hop))
                    server_socket.send(send_string.encode('utf-8'))

        # Print the response to standard output, both as byte stream and decoded text



if __name__ == '__main__':
    run_client()
