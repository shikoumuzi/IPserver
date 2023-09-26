
DIRLINKS = -I../include/ -I../include/tbf/threadtbf/  
LIBLINKS = -std=c++20 -pthread -lpthread
CFLAGS = $(DIRLINKS) $(LIBLINKS)
CC = g++
OPT-RELEASE = -O1 -c -o
OPT-DEBUG = -c -o
DEFAULT_OPERATOR =  $(CC) $(CFLAGS) $^ -c -o $@



Mserver: Mtbf.o medialib.o thr_list.o thr_channel.o server.o
	$(CC) $(CFLAGS) $^ -o $@

Mtbf.o:../include/tbf/threadtbf/Mtbf.cpp
	$(CC) $(LIBLINKS)  $^ ${OPT-DEBUG} $@ 

medialib.o:medialib.cpp
	$(CC) $(CFLAGS) $^  ${OPT-DEBUG} $@

thr_list.o:thr_list.cpp
	$(CC) $(CFLAGS) $^  ${OPT-DEBUG} $@


thr_channel.o: thr_channel.cpp
	$(CC) $(CFLAGS) $^  ${OPT-DEBUG} $@

server.o:server.cpp
	$(CC) $(CFLAGS) $^  ${OPT-DEBUG} $@ 

clean:
	rm -rf *.o Mserver



Mserver-release: Mtbf.o medialib.o thr_list.o thr_channel.o server.o
	$(CC) $(CFLAGS) $^ -o $@

Mtbf.o:../include/tbf/threadtbf/Mtbf.cpp
	$(CC) $(LIBLINKS)  $^ ${OPT-RELEASE} $@ 

medialib.o:medialib.cpp
	$(CC) $(CFLAGS) $^  ${OPT-RELEASE} $@

thr_list.o:thr_list.cpp
	$(CC) $(CFLAGS) $^  ${OPT-RELEASE} $@


thr_channel.o: thr_channel.cpp
	$(CC) $(CFLAGS) $^  ${OPT-RELEASE} $@

server.o:server.cpp
	$(CC) $(CFLAGS) $^  ${OPT-RELEASE} $@ 

clean:
	rm -rf *.o Mserver


