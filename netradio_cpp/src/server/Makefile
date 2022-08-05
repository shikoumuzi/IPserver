
DIRLINKS = -I../include/ -I../include/tbf/threadtbf/  
LIBLINKS = -std=c++20 -pthread -lpthread
CFLAGS = $(DIRLINKS) $(LIBLINKS)
CC = g++

DEFAULT_OPERATOR =  $(CC) $(CFLAGS) $^ -c -o $@


all:Mserver

Mserver: Mtbf.o sockres.o medialib.o thr_list.o thr_channel.o server.o
	$(CC) $(CFLAGS) $^ -o $@

Mtbf.o:../include/tbf/threadtbf/Mtbf.cpp
	$(CC) $(LIBLINKS)  $^ -c -o $@ 

sockres.o:sockres.cpp
	$(DEFAULT_OPERATOR) 

medialib.o:medialib.cpp
	$(CC) $(CFLAGS) $^ -c -o $@

thr_list.o:thr_list.cpp
	$(CC) $(CFLAGS) $^ -c -o $@


thr_channel.o: thr_channel.cpp
	$(CC) $(CFLAGS) $^ -c -o $@

server.o:server.cpp
	$(CC) $(CFLAGS) $^ -c -o $@ 

clean:
	rm -rf *.o Mserver
