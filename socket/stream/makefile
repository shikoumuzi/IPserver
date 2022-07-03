server: server.o
	g++ -pthread $^ -o $@
	rm -rf $^
client: client.o
	g++ -pthread $^ -o $@
	rm -rf $^
clean:
	rm -rf *.o client server
