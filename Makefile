
INC=-I../../unpv13e
LIB=-L../../unpv13e

main: main.cpp
	gcc main.cpp -Wall -lstdc++ -lunp -o main

local: main.cpp
	gcc main.cpp -Wall -DDEBUG_ $(INC) $(LIB) -lstdc++ -lunp -o tftp
