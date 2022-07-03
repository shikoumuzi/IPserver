#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<signal.h>
#include<unistd.h>
#include"proto.h"
#include<pthread.h>
#include<vector>
using namespace std;
vector<pthread_t> vph;
int sd = 0, newsd = 0;

void sdclose(int signum)
{
	close(newsd);
	close(sd);
	for(pthread_t x : vph)
	{
		pthread_cancel(x);
	}
	for(pthread_t x : vph)
	{
		pthread_join(x, NULL);
	}
}

void* sendTime(void* p)
{
	
	int *intptr = (int*)p;
	int sd = *intptr;	
	while(1)
	{
		char buf[1024];
		int len = sprintf(buf, "%lld \r\n", (long long)time(NULL));
		send(sd, buf, len, 0);
		sleep(1);
	}
}
int main()
{
	sigset_t set;
	sigemptyset(&set);

	struct sigaction sigact;
	sigact.sa_handler 	= sdclose;
	sigact.sa_mask		= set;
	sigact.sa_flags		= 0;
	


	struct sockaddr_in socaddr, raddr;
	socaddr.sin_family 	= AF_INET;
	socaddr.sin_port	= htons(atoi(RCVPOT));
	inet_pton(AF_INET, "0.0.0.0", &socaddr.sin_addr);
	
	socklen_t soclen = sizeof(socaddr), rsoclen = sizeof(socaddr);

	sd = socket(AF_INET, SOCK_STREAM, 0);
	
	bind(sd,(struct sockaddr*)&socaddr, soclen);
	
	sigaction(SIGQUIT, &sigact, NULL);
	

	listen(sd, 20);
	//listen中backlog这个参数曾经是作为半连接池数量上限的输入，
	//但由于半连接洪水攻击，现如今则是作为作多可以连接的客户端上限输入
	
	char ipstr[48];
	while(1)
	{	
		newsd = accept(sd, (struct sockaddr*)&raddr, &rsoclen);
		//accpet会返回一个新的文件描述符来进行连连接
		memset(ipstr, '\0', 48);
		inet_ntop(AF_INET, &raddr.sin_addr, ipstr, 48);
		cout<< "Clien: "<< ipstr << ":" << raddr.sin_port << endl;
		
		pthread_t ph;
		pthread_create(&ph, NULL, sendTime, &newsd);
		vph.push_back(ph);

	}

}
