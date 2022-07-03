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
using namespace std;


int main(int arg, char*argv[])
{
	srand((unsigned)time(NULL));
	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	int msglen = strlen(argv[1]) + sizeof(msg_st);
	
	int val = 1;
	if(setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0)
	//第一个参数表示要修改哪个套接字， 第二个参数表示要在哪个层面上进行修改，
	//第三个参数在该层面当中可修改的一些选项，第四个参数是根据不同的选项所要输入的参数，
	//第五个参数是输入参数的长度
	{
		perror("setsockopt():");
		exit(1);
	}
		

	msg_st* msg = (msg_st*)malloc(msglen);
	memcpy(msg->name, argv[1], strlen(argv[1]));
	msg->chinese = htonl(rand()%100);
	msg->math = htonl(rand()%100);
	//主机序转字节序
		
	struct sockaddr_in locaddr; 
	locaddr.sin_family 	= AF_INET;
	locaddr.sin_port	= htons(atoi(RCVPOT));
	
	inet_pton(AF_INET, "255.255.255.255", &locaddr.sin_addr);
	//255.255.255.255是全网广播地址
	//在网络系统的各个协议层次中，存在着很多的协议开关
	//例如在这里，全网广播是被默认不允许发送的
	

	socklen_t addrlen = sizeof(locaddr);
	//对于广播而言，能够接受到也是正确的，不能接受到也是正确的
	while(1)
	{
		sleep(1);
		sendto(sd, msg, msglen, 0,
				(struct sockaddr*)&locaddr, addrlen);
		puts("OK!");	
		msg->chinese = htonl(rand()%100);
		msg->math = htonl(rand()%100);
	
	}
	close(sd);
	return 0;
}
