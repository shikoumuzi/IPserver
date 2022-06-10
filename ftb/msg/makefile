fmsg = ftb_msg
Link = -std=c++20 -pthread -lpthread
rece = receiver
send = sender

$(send): $(fmsg).o $(send).o
	g++ $(Link) -o $@ $^
	make clean
	ctags -R
$(send).o: $(send).s
	g++ -c $^ -o $@

$(send).s: $(send).i1
	g++ -std=c++20 -S $(send).i -o $@
$(send).i1:
	rm -rf $(send).i
	g++ -E $(send).cpp -o $(send).i


$(rece):  $(fmsg).o $(rece).o 
	g++  $(Link) -o $@ $^
	ctags -R
	make clean

$(rece).o: $(rece).s
	g++ -c $^ -o $@

$(rece).s: $(rece).i1
	g++ -std=c++20 -S $(rece).i -o $(rece).s
$(rece).i1:
	rm -rf $(rece).i
	g++ -E $(rece).cpp -o $(rece).i

	
#$(fmsg): $(fmsg).o
#	g++ $(Link) $^ -o $@
#	ctage -R
#	rm -rf $^

$(fmsg).o: $(fmsg).s
	g++ -c $^ -o $@

$(fmsg).s: $(fmsg).i1
	g++ -std=c++20 -S $(fmsg).i -o $@
	rm -rf $(fmsg).i
$(fmsg).i1:
	rm -rf $(fmsg).i
	g++ -E $(fmsg).cpp -o $(fmsg).i
clean: 
	rm -rf *.o *.s *.i 
