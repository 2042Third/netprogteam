#!/usr/bin/env python3

import sys  # For arg parsing
import socket  # For sockets
import select
import queue
import math

MSS = 1024

#Sensors, (range, x,y, socket)
sensors = dict()

# Base stations parsing and storing.
# base_station's elements is a 4-tuple, containing (X,Y,Num of links, [links])
base_stations = dict()

# Returns the euclidean distance given (x1, y1) and (x2, y2)


def dist(x1, x2, y1, y2):
    return math.sqrt((x2-x1)**2 + (y2-y1)**2)


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
    return [n for n in sorted(reachable, key=lambda node: (dist(node[1], destX, node[2], destY), node[0], node))]



def reachable(sen, station, curr_sen):
    """
    @param sen is the dictionary of sensorName => (range, x, y, sock)
    @param station is a dictionary of baseStationName => (x, y, numLinks, [links])
    @param curr_sen is the sensor
    @returns a list of (ID, x, y) of the reachable nodes from the given sensor
    """
    reach = []
    senr, senx, seny = sensors[curr_sen][:3]
    for key in sen:
        if key == curr_sen:
            continue
        else:
            r, x, y = sen[key][:3]
            d = dist(x, senx, y, seny)
            # If reachable to current curr_sen -- originally (d <= r and d <= senr)
            if d <= senr:
                reach.append((key, x, y))
    for key in station:
        x = station[key][0]
        y = station[key][1]
        d = dist(x, senx, y, seny)
        if d <= senr:  # If reachable to current curr_sen
            reach.append((key, x, y))
    return reach

# Takes a list of (ID, x, y) and returns a string of "ID x y ID1 x1 y1 ..."
def reachableToMsg(reach):
    return ' '.join([f"{node[0]} {node[1]} {node[2]}" for node in reach])


def read_message(message, sock):
    """
    Function processes a given message (only UPDATEPOSITION and WHERE)
    @param message is a string representing the message
    @param sensors is a dict of sensorName => (range, x, y, sock)
    @param base_stations is a dictionary of baseStationName => (x, y, numLinks, [links])
    @param sock is the socket that sent the message
    @returns a string for the response message
    """
    global sensors
    global base_stations
    return_message = ''
    msg = message.split()
    if msg[0] == 'UPDATEPOSITION':
        # Store the imformation of the sensor
        sensors[msg[1]] = (int(msg[2]), int(msg[3]), int(msg[4]), sock)
        reach = reachable(sensors, base_stations, msg[1])
        return_message = "REACHABLE {} {}".format(len(reach), reachableToMsg(reach))
    elif msg[0] == 'WHERE':
        if msg[1] in base_stations.keys():  # Base station location
            return_message = 'THERE {} {} {}'.format(
                msg[1], base_stations[msg[1]][0], base_stations[msg[1]][1])
        else:  # It's asking for a sensor location
            return_message = 'THERE {} {} {}'.format(msg[1], sensors[msg[1]][1], sensors[msg[1]][2])

    return return_message

def reachableForBaseStation(baseID, hoplist):
    """
    @param baseID is the base station ID
    @param hoplist is the list of ID's that have already been visited
    @returns a list of (ID, x, y) of reachable nodes (base station/sensor) from the baseID that are not in hoplist
    """
    reach = [] # All reachable base stations and sensors from the current baseID
    for name in base_stations[baseID][-1]: # All direct base stations to our baseID
        data = base_stations[name]
        reach.append((name, data[0], data[1])) # ID, x, y
    reach = reach + [(sen, data[1], data[2]) for sen, data in sensors.items()] # all sensors
    reachableNotVisited = list(set([node[0] for node in reach]) - set(hoplist))
    result = []
    # print(reachableNotVisited)
    # print(reach)
    for id in reachableNotVisited:
        for node in reach:
            if node[0] == id:
                result.append(node)
    return result

def processDataMessage(message):
    """
    Function processes the logic for a DataMessage
      - it can simulate all the base station hops until the next sensor
      - it will relay the message to the sensor, if possible, if dest is
        a sensor
    @param message is a string of the DATAMESSAGE format
    """
    
    originID, nextID, destID = message.split()[1:4]
    hoplist = message.split()[5:]
    if nextID in sensors.keys():  # Sensor -> Controller -> Sensor
        sensorSock = sensors[nextID][-1]
        if nextID == destID:
            _, destX , destY, _ = sensors[destID] 
            sortWhole = [(dist(sensors[originID][1],base_stations[x][0],sensors[originID][2], base_stations[x][1]), x) 
                            for x in base_stations.keys()
                            if dist(destX,base_stations[x][0],destY, base_stations[x][1]) <= sensors[originID][0]]
            sortWhole = sorted(sortWhole)
            # print(f"{sortWhole[0][1]}: Message from {originID} to {destID} being forwarded through {sortWhole[0][1]}", flush=True)
        sensorSock.send(message.encode('utf-8'))
    else:  # Sensor -> Controller -> Base Station chain -> Sensor
        sendMsg = True # bool of whether we should send DATAMESSAGE to sensor after the chain
        while nextID not in sensors.keys():
            baseID = nextID  # Pretend we are baseID
            
            if destID == baseID: # It arrived
                print(
                    f'{baseID}: Message from {originID} to {destID} succesfully received.', flush=True)
                sendMsg = False
                break
            else:
                # Insert here
                reachableNotVisited = reachableForBaseStation(baseID, hoplist)
                if len(reachableNotVisited) == 0:
                    print(f"{baseID}: Message from {originID} to {destID} could not be delivered.", flush=True)
                    sendMsg = False
                    break
                else: # "Send" Datamessage to next base station
                    destObj = (sensors[destID][1], sensors[destID][2]) if (destID in sensors.keys() ) else (base_stations[destID][0], base_stations[destID][1])
                    nextList = sortedByDist(reachableNotVisited, destObj)
                     # Sorted by dist of all reachable to the destination node
                    filtered = []
                    # print(nextList)
                    # print(filtered)
                    if destID in sensors.keys():
                        for x in nextList:
                            if x[0] in sensors.keys():
                                _,senx,seny = x
                                bx,by,_,_ = base_stations[baseID]
                                
                                if not sensors[x[0]][0] >= dist(senx,bx,seny,by):
                                    continue
                                else:
                                    filtered.append(x)
                            else:
                                filtered.append(x)
                    else:
                        filtered = nextList
                    
                    if (len(filtered) == 0):
                        print(f"{baseID}: Message from {originID} to {destID} could not be delivered.", flush=True)
                        sendMsg = False
                        break
                    nextID = filtered[0][0]
                    hoplist.append(baseID)
                    if originID == baseID and nextID == destID:
                        print(f"{baseID}: Sent a new message directly to {destID}", flush=True)
                    elif originID == baseID:
                        print(f"{baseID}: Sent a new message bound for {destID}", flush=True)
                    else:
                        print(f"{baseID}: Message from {originID} to {destID} being forwarded through {baseID}", flush=True)
                    
        if sendMsg: # Send to the sensor
            sensorSock = sensors[nextID][-1]
            msg = f"DATAMESSAGE {originID} {nextID} {destID} {len(hoplist)} {' '.join(hoplist)}"
            sensorSock.send(msg.encode('utf-8'))

# Essentially main()
def run_server():
    global sensors
    global base_stations
    if len(sys.argv) != 3:
        print(
            f"Proper usage is {sys.argv[0]} [port number] [Base Station file]")
        sys.exit(0)

    # Create a TCP socket
    listening_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # Set the socket to listen on any address, on the specified port
    listening_socket.bind(('', int(sys.argv[1])))
    listening_socket.listen(5)

    # read set
    rset = [listening_socket, sys.stdin]

    # Read base station file
    with open(sys.argv[2], 'r') as file:
        for l in file:
            line = l.split()
            base_stations[line[0]] = (int(line[1]), int(line[2]), int(
                line[3]), line[4:])

    # Server loop
    while rset:
        # readable, writable, exceptional
        rable = select.select(rset, [], [])[0]
        # There are messages that can be read
        for sock in rable:
            if sock is listening_socket:  # Adding a new sensor
                (client_socket, addr) = sock.accept()
                rset.append(client_socket)
            else:
                if sock is sys.stdin:
                    cmd = sys.stdin.readline()
                    if cmd.split()[0] == 'QUIT':
                        for s in rset:
                            s.close()
                        listening_socket.close()
                        return
                    else:  # SENDDATA
                        originID = cmd.split()[1] #Will never be a sensor
                        destID = cmd.split()[2]
                        if originID == 'CONTROL': # originID is CONTROL, all base stations reachable, nextID must be a base station
                            if destID in sensors.keys():
                                nextID = sortedByDist([(name, base[0], base[1]) for name, base in base_stations.items()], sensors[destID][1:3])[0][0]
                                processDataMessage(["DATAMESSAGE", originID, nextID, destID, None, []])
                            else: # It's a base station so just jet it to them
                                print(f"{originID}: Sent a new message directly to {destID}")
                                print(f"{destID}: Message from {originID} to {destID} succesfully recieved.")
                        else: # originID is a base station, next hop should be based on what is reachable from origin base station
                            reachableNotVisited = reachableForBaseStation(originID, []) # (Id, x, y) of nodes (base/sensor) reachable from originID
                            nextID = ''
                            if destID in sensors.keys():
                                nextID = sortedByDist(reachableNotVisited, sensors[destID][1:3])[0][0]
                            else: # destID is base station
                                nextID = sortedByDist(reachableNotVisited, (base_stations[destID][0], base_stations[destID][1]))[0][0]
                            processDataMessage(["DATAMESSAGE", originID, nextID, destID, 1, [originID]])
                            
                else:  # Receive message from sensor
                    message = sock.recv(MSS).decode("utf-8")
                    # print(message)
                    if message != '':
                        if message.split()[0] != 'DATAMESSAGE':
                            send_msg = read_message(message, sock)
                            sock.send(send_msg.encode('utf-8'))
                        else:  # DATAMESSAGE
                            processDataMessage(message)

                    else:  # client connection closed for some reason, which should not be possible given the systme model
                        rset.remove(sock)
                        sock.close()


if __name__ == '__main__':
    run_server()
