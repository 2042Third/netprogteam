default: client server

client: echo_client.cpp
	gcc echo_client.cpp libunp.a -o lab3_client.out

server: echo_server.cpp
	gcc echo_server.cpp libunp.a -o lab3_server.out

clean:
	rm -f *.o *.out
