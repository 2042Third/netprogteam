#!/usr/bin/env python3

import sys  # For arg parsing
import socket  # For sockets
import select, queue

def run_server():
    if len(sys.argv) != 2:
        print(f"Proper usage is {sys.argv[0]} [port number]")
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
    #msg queue (dictionary)
    mque = {}

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
