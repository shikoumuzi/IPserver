
pooltext: ftb_msg.o sockpropool.o pooltext.o
	g++ -std=c++20 -pthread $^ -o $@
	rm *.o
ftb_msg.o: ../../ftb/msg/ftb_msg.cpp
	g++ -std=c++20 -c -o $@ $^
		
sockpropool: sockpropool.o
	g++ -pthread $^ -o $@

sockpropool.o:sockpropool.cpp
	g++ -std=c++20 -c -o $@ $^

pooltext.o:pooltext.cpp
	g++ -std=c++20 -c -o $@ $^
