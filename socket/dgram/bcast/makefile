CC = g++

socrece: socrece.o
	$(CC) $^ -o $@

socsend: socsend.o
	$(CC) $^ -fPIE -o $@

clean:
	rm -rf *.o socrece socsend 
