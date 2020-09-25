
INC=-I../../unpv13e
LIB=-L../../unpv13e

main: main.cpp
	g++ -Wall $(INC) $(LIB) -lstdc++ -lunp -o main main.cpp
