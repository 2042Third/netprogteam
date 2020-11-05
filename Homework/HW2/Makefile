INC=-I../../../netprog/unpv13e/lib
LIB=-L../../../netprog/unpv13e

main: word_guess.cpp
	clang word_guess.cpp libunp.a -lstdc++ -o word_guess.out

test: word_guess.cpp
	gcc word_guess.cpp $(INC) $(LIB) -lunp -lstdc++ -DDEBUG -o word_guess.out