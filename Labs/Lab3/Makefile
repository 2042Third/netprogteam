default: client server

client: lab3_client.cpp
	gcc lab3_client.cpp libunp.a -o lab3_client.out

server: lab3_server.cpp
	gcc lab3_server.cpp libunp.a -o lab3_server.out

clean:
	rm -f *.o *.out

local: clientLocal serverLocal

INC=-I../../../unpv13e/lib
LIB=-L../../../unpv13e

clientLocal: lab3_client.cpp
	gcc lab3_client.cpp -o clientLocal.out -lstdc++ -DDEBUG_ ${INC} ${LIB} -lunp -Wall

serverLocal: lab3_server.cpp
	gcc lab3_server.cpp -o serverLocal.out -lstdc++ ${INC} ${LIB} -lunp -Wall
