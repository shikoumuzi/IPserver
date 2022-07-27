all: Mtoken

Mtoken:  Mtoken.o main.o
	g++ -std=c++17 $^ -o $@ #这里编译时必须要把头文件先编译，保证编译顺序
clean:
	rm -rf *.o

