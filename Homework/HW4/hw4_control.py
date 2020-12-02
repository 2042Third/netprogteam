#!/usr/bin/env python3

import sys  # For arg parsing
import socket  # For sockets
import select, queue
import math

# Returns the euclidean distance given (x1, y1) and (x2, y2)
def dist(x1,x2,y1,y2):
    return math.sqrt((x2-x1)**2 + (y2-y1)**2)

"""
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
@param sen is the dictionary of sensorName => (range, x, y)
@param station is a dictionary of baseStationName => (x, y, numLinks, [links])
@param cur_sen is the current sensor's name
@returns a list of strings, each string having the format of "ID x y"
"""
def reachable(sen, station, cur_sen):
    reach = []
    senr, senx, seny = sen[cur_sen] # Current sensor's range, x, y
    for key in sen:
        if key == cur_sen:
            continue
        else:
            r,x,y = sen[key]
            d = dist(x,senx,y,seny)
            if d <= senr: # If reachable to current sensor
                reach.append('{} {} {}'.format(key, x, y))
    for key in station:
        x = station[key][0]
        y = station[key][1]
        d = dist(x,senx,y,seny)
        if d <= senr: # If reachable to current sensor
            reach.append('{} {} {}'.format(key,x,y))
    return reach


def read_message(message, sensors, base_stations, sock):
    return_message = ''
    msg = message.split()
    print(msg)
    if msg[0] == 'UPDATEPOSITION':
        sensors[msg[1]] = (int(msg[2]), int(msg[3]), int(msg[4]), sock )#Store the imformation of the sensor 
        reach = reachable(sensors, base_stations, msg[1])
        return_message = "REACHABLE {} {}".format(len(reach),' '.join(reach))
    elif msg[0] == 'QUIT':
        return_message = ''
    elif msg[0] == 'WHERE':
        return_message = 'THERE {} {} {}'.format(
            msg[1], sensors[msg[1]][1], sensors[msg[1]][2])
    return return_message

def run_server():
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

    #Sensors, (range, x,y, socket)
    sensors = dict()

    #Base stations parsing and storing.
    #base_station's elements is a 4-tuple, containing (X,Y,Num of links, [links])
    base_stations = dict()
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
            if sock is listening_socket:
                (client_socket, address) = sock.accept()
                client_socket.setblocking(0)
                rset.append(client_socket)
                mque[client_socket] = queue.Queue()
            else:
                if sock is sys.stdin:
                    if input() == 'QUIT':
                        return
                message = sock.recv(1024)
                if message:
                    send_msg = read_message(message.decode('utf-8'), sensors, base_stations, sock)
                    #SEND MESSAGE:


                    # mque[sock].put(send_msg)# What message gets sent
                    # print("server tries to send",read_message(message, sensors, base_stations))
                    # print(f"Server received {len(message)} bytes: \"{message}\"")

                    # if (sock not in wset and sock is not sys.stdin):
                    #     wset.append(sock) # Which client to send to.
                else:
                    rset.remove(sock)
                    sock.close()
                    del mque[sock] 
# #There are messages that can be sent
#         for sock in wable:
#             try:
#                 next_msg = mque[sock].get_nowait() 
#             except :
#                 wset.remove(sock)
#             else: 
#                 print(f"Server sending {len(next_msg)} bytes: \"{next_msg}\"")
#                 sock.send(next_msg.encode('utf-8'))
# #Exceptions
#         for sock in excp:
#             rable.remove(sock)
#             if sock in wset:
#                 wset.remove(sock)
#             sock.close()
#             del mque[sock]



if __name__ == '__main__':
    run_server()
