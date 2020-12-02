#!/usr/bin/env python3

import sys  # For arg parsing
import socket  # For sockets
import select, queue
import math

def dist(x1,x2,y1,y2):
    return math.sqrt((x2-x1)**2 + (y2-y1)**2)

def reachable(sen, station, cur_sen):
    reach = []
    senr, senx, seny = sen[cur_sen]
    for key in sen:
        if key == cur_sen:
            continue
        else:
            r,x,y = sen[key]
            d = dist(x,senx,y,seny)
            if (d <= r and d <= senr):
                reach.append('{} {} {}'.format(key, x,y))
    for key in station:
        x = station[key][0]
        y = station[key][1]
        d = dist(x,senx,y,seny)
        if d <= senr:
            reach.append('{} {} {}'.format(key,x,y))
    return reach


def read_message(message, sensors, base_stations):
    return_message = ''
    msg = message.split()
    print(msg)
    if msg[0] == 'UPDATEPOSITION':
        sensors[msg[1]] = (int(msg[2]), int(msg[3]), int(msg[4]))
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
    # bind takes a 2-tuple, not 2 arguments
    listening_socket.bind(('', int(sys.argv[1])))
    listening_socket.listen(5)
    
    #read set
    rset = [listening_socket, sys.stdin]
    #write set
    wset = []
    #msg queue 
    mque = {}

    #Sensors
    sensors = dict()

    #Base stations parsing and storing.
    #base_station's elements is a 4-tuple, containing (X,Y,Num of links, [links])
    base_stations = dict()
    with open(sys.argv[2], 'r') as file:
        for l in file:
            line = l.split()
            base_stations[line[0]] = (int(line[1]),int(line[2]),int(line[3]), [line[y] for y in range(4, len(line)-1)]) #thanks prog lang :)


    # Server loop
    while rset: 
        # readable, writable, exceptional
        rable, wable, excp = select.select(rset, wset, rset)
        
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
                    send_msg = read_message(message.decode('utf-8'), sensors, base_stations)
                    
                    mque[sock].put(send_msg)
                    print("server tries to send",read_message(message, sensors, base_stations))
                    print(f"Server received {len(message)} bytes: \"{message}\"")

                    if (sock not in wset and sock is not sys.stdin):
                        wset.append(sock)
                else:
                    if sock in wset:
                        wset.remove(sock)
                    rset.remove(sock)
                    sock.close()
                    del mque[sock] 

        for sock in wable:
            try:
                next_msg = mque[sock].get_nowait() 
            except :
                wset.remove(sock)
            else: 
                print(f"Server sending {len(next_msg)} bytes: \"{next_msg}\"")
                sock.send(next_msg.encode('utf-8'))

        for sock in excp:
            rable.remove(sock)
            if sock in wset:
                wset.remove(sock)
            sock.close()
            del mque[sock]



if __name__ == '__main__':
    run_server()
