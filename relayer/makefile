
relayer: relayer.o main.o
	g++ -g -std=c++2a  $^ -pthread -lpthread -o $@
	ctags -R

clean: 
	rm -rf *.o relayer
