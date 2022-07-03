#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<string.h>
#include"proto.h"

int main()
{
	int socfd = 0;
	socfd = socket(AF_INET, SOCK_DGRAM, 0);
	/*对于报式套接字来说 默认为UDP， 流式套接字来说 默认为TCP*/
	//AF_INET确认ipv4协议组， SOCK_DFRAM流式传输方式， 0 表示选择默认的协议传输方式 
	//确定协议组和发送接受方式
	if(socfd < 0)
	{
		perror("socket ");
		exit(1);
	}
	
	struct sockaddr_in locadd, rmadd;

        locadd.sin_family 	= AF_INET;
	locadd.sin_port		= htons(atoi(RCVPOT));//该参数作为端口绑定参数
	inet_pton(AF_INET, "0,0,0,0", &locadd.sin_addr);
	//该函数为点分式转换成大整数
	//当中间的地址为 0，0，0，0的话意味着anyaddress 可以自动匹配自身的地址（回环网络）
	if(bind(socfd, (sockaddr*)(&locadd), sizeof(locadd)) < 0)
	//中间的参数类型是捏造出来的，需要自己去强转void* 或者 struct socaddr*
	//通过sizeof来确定类型
	{
		perror("bind()");
		exit(1);
	}

	int msglen = sizeof(msg_st) + MAXSIZE -1;
	msg_st* msg = (msg_st*)malloc(msglen);	
	socklen_t rmaddlen = sizeof(rmadd);
	//这里一定要进行初始化擦作，不然recvfrom函数第一次打印出来的地址报错

	char ipstr[100];
	while(1)
	{
		//recv 是建立在已经建立好连接的前提下
		recvfrom(socfd, msg, msglen, 0, (sockaddr*)&rmadd, &rmaddlen);
		//recvfrom 是可以在没有连接的前提下连接
		//接受信息
		memset(ipstr, '\0', 100);
		inet_ntop(AF_INET, &rmadd.sin_addr,ipstr, 100 );
		printf("--MSG FROM-- %s:%d\n", ipstr , rmadd.sin_port);
		printf("--NAME-- %s\n", msg->name);//单字节传输中不涉及大小端转换
		printf("--MATH-- %d\n", ntohl(msg->math));
		printf("--CHINESE-- %d\n", ntohl(msg->chinese));
	}

	close(socfd);
	return 0;
}



