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
#include<time.h>
using namespace std;
int sd = 0, newsd = 0;

void sdclose(int signum)
{
	close(newsd);
	close(sd);
}

void sendTime(int sd)
{
	char buf[1024];
	int len = sprintf(buf, "%lld \r\n", (long long)time(NULL));
	send(sd, buf, len, 0);
}

int main(int arg, char* argv[])
{
	if(arg < 3)
	{
		perror("Usage...");
		exit(1);
	}
	sigset_t set;
	sigemptyset(&set);

	struct sigaction sigact;
	sigact.sa_handler 	= sdclose;
	sigact.sa_mask		= set;
	sigact.sa_flags		= 0;
	
	struct sockaddr_in socaddr, raddr;
	socaddr.sin_family 	= AF_INET;
	socaddr.sin_port	= htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &socaddr.sin_addr);
	
	socklen_t soclen = sizeof(socaddr), rsoclen = sizeof(socaddr);

	sd = socket(AF_INET, SOCK_STREAM, 0);
	
	sigaction(SIGQUIT, &sigact, NULL);

	connect(sd, (struct sockaddr*)&socaddr, soclen);
	
		
	char ipstr[48];
	FILE *fp = fdopen(sd, "r+");
	//将sd转换为标准io进行操作，
	//此时从该sd上获取的数据和写入的数据均可以采用标准io操作
	
	time_t stamp = 0;
	struct tm *p = nullptr;
	while(1)
	{
		sleep(1);
		fscanf(fp, "%lld",&stamp);
		p = gmtime(&stamp);
		fprintf(stdout, "%d:%d:%d:%d:%d:%d\n", 
				p->tm_year, p->tm_mon, p->tm_mday, p->tm_hour, 
				p->tm_min,p->tm_sec);		
	}

}
