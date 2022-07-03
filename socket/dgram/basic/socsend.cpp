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
	int msglen = strlen(argv[2]) + sizeof(msg_st);

	msg_st* msg = (msg_st*)malloc(msglen);
	memcpy(msg->name, argv[2], strlen(argv[2]));
	msg->chinese = htonl(rand()%100);
	msg->math = htonl(rand()%100);
	//主机序转字节序
		
	struct sockaddr_in locaddr; 
	locaddr.sin_family 	= AF_INET;
	locaddr.sin_port	= htons(atoi(RCVPOT));
       	inet_pton(AF_INET, argv[1], &locaddr.sin_addr);
	
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
