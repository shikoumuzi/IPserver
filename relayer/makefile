
relayer: relayer.o main.o
	g++ -std=c++2a  $^ -pthread -o $@
	ctags -R

clean: 
	rm -rf *.o relayer
