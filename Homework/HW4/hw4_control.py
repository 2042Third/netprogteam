#!/usr/bin/env python3

import sys  # For arg parsing
import socket  # For sockets
import select, queue
import math

MSS = 1024

#Sensors, (range, x,y, socket)
sensors = dict()

#Base stations parsing and storing.
#base_station's elements is a 4-tuple, containing (X,Y,Num of links, [links])
base_stations = dict()

# Returns the euclidean distance given (x1, y1) and (x2, y2)
def dist(x1,x2,y1,y2):
    return math.sqrt((x2-x1)**2 + (y2-y1)**2)

"""
@requres reachable is a list of all in range sensors/base stations
@param reachable is a list of (id, x, y)
@param dest is the destination base station or sensor (id, x, y)
@returns a list of the reachable sorted by their distance to the dest
         breaking ties using their id's in lexical order
"""
def sortedByDist(reachable, dest):
    destX, destY = dest[1:]
    # Sorts by distance then lexical. Then only return the reachable list that was sorted.
    return [n[-1] for n in sorted(reachable, key=lambda node: (dist(node[1], destX, node[2], destY), node[0], node))]

"""
@param sen is the dictionary of sensorName => (range, x, y, sock)
@param station is a dictionary of baseStationName => (x, y, numLinks, [links])
@param cur_sen is the current sensor's name
@returns a list of strings, each string having the format of "ID x y"
"""
def reachable(sen, station, cur_sen):
    reach = []
    senr, senx, seny = sen[cur_sen][:3] # Current sensor's range, x, y
    for key in sen:
        if key == cur_sen:
            continue
        else:
            r,x,y = sen[key][:3]
            d = dist(x,senx,y,seny)
            if d <= senr: # If reachable to current sensor -- originally (d <= r and d <= senr)
                reach.append('{} {} {}'.format(key, x, y))
    for key in station:
        x = station[key][0]
        y = station[key][1]
        d = dist(x,senx,y,seny)
        if d <= senr: # If reachable to current sensor
            reach.append('{} {} {}'.format(key,x,y))
    return reach


"""
Function processes a given message (only UPDATEPOSITION and WHERE)
@param message is a string representing the message
@param sensors is a dict of sensorName => (range, x, y, sock)
@param base_stations is a dictionary of baseStationName => (x, y, numLinks, [links])
@param sock is the socket that sent the message
@returns a string for the response message
"""
def read_message(message, sock):
    global sensors
    global base_stations
    return_message = ''
    msg = message.split()
    print(msg)
    if msg[0] == 'UPDATEPOSITION':
        sensors[msg[1]] = (int(msg[2]), int(msg[3]), int(msg[4]), sock )#Store the imformation of the sensor 
        reach = reachable(sensors, base_stations, msg[1])
        return_message = "REACHABLE {} {}".format(len(reach),' '.join(reach))
    elif msg[0] == 'WHERE':
        if msg[1] in base_stations.keys(): # Base station location
            return_message = 'THERE {} {} {}'.format(msg[1], base_stations[msg[1]][1], base_stations[msg[1]][2])
        else: # It's asking for a sensor location
            return_message = 'THERE {} {} {}'.format(msg[1], sensors[msg[1]][1], sensors[msg[1]][2])
            
    return return_message

def run_server():
    global sensors
    global base_stations
    if len(sys.argv) != 3:
        print(f"Proper usage is {sys.argv[0]} [port number] [Base Station file]")
        sys.exit(0)

    # Create a TCP socket
    listening_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # Set the socket to listen on any address, on the specified port
    listening_socket.bind(('', int(sys.argv[1])))
    listening_socket.listen(5)
    
    #read set
    rset = [listening_socket, sys.stdin]
    # wable = []
    # excp = []
    #msg queue 
    mque = {}

    with open(sys.argv[2], 'r') as file:
        for l in file:
            line = l.split()
            base_stations[line[0]] = (int(line[1]),int(line[2]),int(line[3]), [line[y] for y in range(4, len(line))]) #thanks prog lang :)


    # Server loop
    while rset: 
        # readable, writable, exceptional
        rable = select.select(rset, [], [])[0]
        # There are messages that can be read
        for sock in rable:
            if sock is listening_socket: # Adding a new sensor
                (client_socket, address) = sock.accept()
                rset.append(client_socket)
            else:
                if sock is sys.stdin:
                    cmd = input()
                    if cmd == 'QUIT':
                        for s in rset:
                            s.close()
                        listening_socket.close()
                        return
                    else: # SENDDATA
                        pass
                else: # Receive message from sensor
                    message = sock.recv(MSS)
                    if message != '':
                        if message[0] != 'DATAMESSAGE':
                            send_msg = read_message(message.decode('utf-8'), sock)
                            sock.send(send_msg.encode('utf-8'))
                        else: # DATAMESSAGE
                            originID, nextID, dest, hoplen = message[1:5]
                            hoplist = message[5:]
                            if nextID in sensors.keys(): # Sensor -> Controller -> Sensor
                                sensorSock = sensors[nextID][-1]
                                sensorSock.send(message.encode('utf-8'))
                            else: # Sensor -> Controller -> Base Station chain -> Sensor
                                baseID = nextID # Pretend we are baseID
                                if 
                                while nextId not in sensors:
                                # We know it's Sensor -> Controller -> Base Station chain
                                # - If destinationid === baseid, print destination reached
                                # - else
                                #     - if all reachable sensors and base stations hit, say can't deliver
                                #     - else
                                #     - Send Datamessage to next hop
                                #         - print if you forwarded it, sent it to the destination, or are just starting the delivery (originID === baseID)
                                if dest

                            

                    else: # client connection closed for some reason, which should not be possible given the systme model
                        rset.remove(sock)
                        sock.close()


if __name__ == '__main__':
    run_server()