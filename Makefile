
INC=-I../../unpv13e/lib
LIB=-L../../unpv13e

tftp.out: main.cpp
	g++ main.cpp -Wall libunp.a -o tftp.out

local: main.cpp
	gcc main.cpp -Wall -DDEBUG_ $(INC) $(LIB) -lstdc++ -lunp -o tftp
