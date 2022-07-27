
CFLAGS = -std=c++20 -I../include/ -I../include/tbf/threadtbf/ -pthread -lpthread

CC = g++


all:server

server:medialib.o thr_list.o thr_channel.o server.o
	v$(CC) $^ -o $@


medialib.o:medialib.cpp
	$(CC) $(CFLAGS) $^ -c -o $@

thr_list.o:thr_list.cpp
	$(CC) $(CFLAGS) $^ -c -o $@


thr_channel.o: thr_channel.cpp
	$(CC) $(CFLAGS) $^ -c -o $@

server.o:server.cpp
	$(CC) $(CFLAGS) $^ -c -o $@ 

clean:
	rm -rf *.o
