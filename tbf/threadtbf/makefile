all: Mtbf

Mtbf:  Mtbf.o main.o
	g++ -O0 -std=c++17 -pthread -lpthread $^ -o $@ 
	##这里编译时必须要把头文件先编译，保证编译顺序
	ctags -R
clean:
	rm -rf *.o Mtbf
