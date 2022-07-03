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
#include<net/if.h>
#include"proto.h"
using namespace std;


int main(int arg, char*argv[])
{
	srand((unsigned)time(NULL));
	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	int msglen = strlen(argv[1]) + sizeof(msg_st);
	
	struct ip_mreqn mreq;
	//这里创建多播组所用到的数据结构取决于手册中加入多播组IP_ADD_MEMEBERSHIP
	//中所规定数据结构

	inet_pton(AF_INET, MCAST, &mreq.imr_multiaddr);
	//输入多播组组号
	inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
	//填写自己的网络地址
	mreq.imr_ifindex = if_nametoindex("eth0");
	//绑定路由器的网络序列号


	if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0)
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
	
	inet_pton(AF_INET, MCAST, &locaddr.sin_addr);
	//多播组都是d类地址，需要创建 加入多播组	
	//在多播中，224.0.0.1默认为所有支持多播的设备所在的组，因此可以通过这个地址实现个广播的效果
	socklen_t addrlen = sizeof(locaddr);
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
