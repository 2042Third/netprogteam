#!/usr/bin/env python3

import sys  # For arg parsing
import socket  # For sockets
import select, queue

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
    rset = [listening_socket]
    #write set
    wset = []
    #msg queue 
    mque = {}

    #Base stations parsing and storing.
    #base_station's elements is a 4-tuple, containing (X,Y,Num of links, [links])
    base_stations = dict()
    with open(sys.argv[2], 'r') as file:
        for l in file:
            line = l.split()
            base_stations[line[0]] = (line[1],line[2],line[3], [line[y] for y in range(4, len(line)-1)]) #thanks prog lang :)


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
                message = sock.recv(1024)
                if message:
                    mque[sock].put(message)

                    print(f"Server received {len(message)} bytes: \"{message}\"")
                    if sock not in wset:
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
                sock.send(next_msg)

        for sock in excp:
            rable.remove(sock)
            if sock in wset:
                wset.remove(sock)
            sock.close()
            del mque[sock]



if __name__ == '__main__':
    run_server()
