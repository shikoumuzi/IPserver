CFLAGS +=-std=c++20 -I../include/ -pthread 

all:clientmain

clientmain:client.o clientmain.o
	g++ $(CFLAGS) $^ -o $@
	rm *.o

clientmain.o:clientmain.cpp
	g++ $(CFLAGS) $^  -c -o $@

client.o:client.cpp
	g++ $(CFLAGS) $^ -c -o $@



clean:
	rm -rf *.o client
