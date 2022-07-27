CFLAGS +=-std=c++20 -I../include/ -Wall 

all:client

client:clientmain.o
	g++ $^  -o $@
	rm *.o
clientmain.o:clientmain.cpp
	g++ $(CFLAGS) $^ -c -o $@

clean:
	rm -rf *.o client
