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

int main()
{
	int socfd = 0;
	socfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(socfd < 0)
	{
		perror("socket ");
		exit(1);
	}

	struct ip_mreqn mreq;
	//这里创建多播组所用到的数据结构取决于手册中加入多播组IP_ADD_MEMEBERSHIP
	//中所规定数据结构

	inet_pton(AF_INET, MCAST, &mreq.imr_multiaddr);
	//输入多播组组号
	inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
	//填写自己的网络地址
	mreq.imr_ifindex = if_nametoindex("eth0");
	//绑定路由器的网络序列号


	setsockopt(socfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));	
	struct sockaddr_in locadd, rmadd;

        locadd.sin_family 	= AF_INET;
	locadd.sin_port		= htons(atoi(RCVPOT));//该参数作为端口绑定参数
	inet_pton(AF_INET, "0.0.0.0", &locadd.sin_addr);
	
	if(bind(socfd, (sockaddr*)(&locadd), sizeof(locadd)) < 0)
	{
		perror("bind()");
		exit(1);
	}

	int msglen = sizeof(msg_st) + MAXSIZE -1;
	msg_st* msg = (msg_st*)malloc(msglen);	
	socklen_t rmaddlen = sizeof(rmadd);

	char ipstr[100];
	while(1)
	{
		recvfrom(socfd, msg, msglen, 0, (sockaddr*)&rmadd, &rmaddlen);
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



