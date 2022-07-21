#include<sys/types.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<getopt.h>
#include<glob.h>
#include<errno.h>
#include"client.h"
/*
 *-M --mgroup: 指定多播组
 *-P --port: 指定接收端口
 *-p --player: 指定解码器
 *-H --help: 帮助
*/


static int writen(int pd, const char *buff, int buffsize)
{
	int len = buffsize, ret = 0, pos = 0;
	while(len > 0)
	{
		ret = write(pd, buff + pos, buffsize);
		if(ret < 0)
		{
			if(errno == EINTR)
				continue;
			perror("write():");
			return -1;
		}
		len -= ret;
		pos += ret;

	}
	return 0;
}



int main(int argc, char* argv[])
{

/*
 *初始化
 *级别：命令行参数 环境变量 配置文件 默认值
 *
 * */
	int c = 0, index = 0;
	struct option cmdopt[] = {
			{"mgroup", '1', NULL , 'M'},
			{"port",'1',NULL, 'P'},
			{"player",'1',NULL,'P'},
			{"help",'0',NULL,'H'},
			{NULL, 0, NULL, 0}};
	int pd[2];
	pid_t pid;

	while(1)
	{
		c = getopt_long(argc, argv, "M:P:p:H", cmdopt, &index);
		if(c < 0)
		{
			break;
		}
		switch(c)
		{
		case 'M':{
				client_conf.mgroup = optarg;
				break;
			}
		case 'P':{
				client_conf.rcvport = optarg;
				break;
			}
		case 'p':{
				client_conf.player_cmd = optarg;
				break;
			}
		case 'H':{
				ClientHelpPrintf();
				exit(0);
			}
		default:
			 printf("Using option truely");
			 abort();
			 break;
		}

	}
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0)
	{
		perror("socket():");
		exit(1);
	}

	struct ip_mreqn mreq;
	inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
	inet_pton(AF_INET, client_conf.mgroup, &mreq.imr_multiaddr);
	mreq.imr_ifindex = if_nametoindex("eth0");
	
	if(setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
		perror("setsockopt(): IP_ADDMEMEBERSHIP: ");
		exit(1);
	}
	
	bool val = 1;
	if(setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) < 0)
	{
		perror("setsockopt(): IP_MULITICAST_LOOP :");
		exit(1);
	}


	struct sockaddr_in laddr ,raddr, serveraddr;
	laddr.sin_family	= AF_INET;
	laddr.sin_port		= htons(atoi(DEFAULT_RCVPORT));
	inet_pton(AF_INET, "0.0.0.0" , &laddr.sin_addr);
	socklen_t serveraddrlen = sizeof(serveraddr);
	socklen_t raddrlen	= sizeof(raddr);

	if(bind(sockfd, (struct sockaddr*)&laddr, sizeof(laddr) ) < 0)
	{
		perror("bind():");
		exit(1);
	}
	

	if(pipe(pd) < 0)
	{
		perror("pipe() : ");
		exit(1);
	}
	
	pid = fork();

	
	//子进程：调用解码器
	//父进程：从网络上收包，发送给子进程
	if(pid < 0)
	{
		perror("fork():");
		exit(1);
	}

	if(pid == 0)
	{
		close(sockfd);
		close(pd[1]);
		close(0);
		dup2(pd[0], 0);
		if(pd[0] > 0)
		{
			close(pd[0]);
		}	
		
		//可以通过shell来执行播放器命令
		execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL);
		perror("execl");
		exit(1);
	}
	
	close(pd[0]);
	//父进程从网络上收包发给子进程
	//收节目单
	msg_list_t *msg_list = (msg_list_t*)malloc(MSG_LIST_MAX);
	if(msg_list == NULL)
	{
		perror("msg_list: malloc():");
		exit(1);
	}

		
	int packlen = 0;
	while(1)
	{
		if (( packlen = recvfrom(sockfd, msg_list, MSG_LIST_MAX, 0,
		       (struct sockaddr*)&serveraddr, &serveraddrlen) ) < sizeof(msg_list_t))
		{
			fprintf(stderr, "message is too small.\n");
			continue;
		}
		if(msg_list->chnid != LISTCHNID )
		{
			fprintf(stderr, "chnid is no match.\n");
			continue;
		}
		break;
	}	

	//打印节目单
	msg_listentry_t *pos = msg_list->entry;
	for(; (char*)pos < ((char*)msg_list + ntohs(pos->len)) ; 
			pos = (msg_listentry_t*)(((char*)pos) + ntohs(pos->len)))
	//通过强转char*保证指针运算一个字节一个字节相加
	//由于这里是变长结构体套变长结构体，所以需要其中一个len数据长度来确定偏移量
	{
		printf("channel: %3ld : %s\n", pos->chnid, pos->desc);
	}

	//选择频道
	int chosenid = 0;
	while(1)
	{
		int ret = scanf("%d", &chosenid);
		if(ret != 1)
		{
			perror("input can not scanf");
			exit(1);
		}
	}

	//收频道包发给子进程
	msg_channel_t* msg_channel = (msg_channel_t*)malloc(MSG_CHANNEL_MAX);
	if(msg_channel == NULL)
	{
		perror("msg_channel: mallock():");
		exit(1);
	}

	packlen = 0;	
	while(1)
	{
		if((packlen = recvfrom(sockfd, msg_channel, MSG_CHANNEL_MAX, 0,
				(struct sockaddr*)&raddr, &raddrlen)) < sizeof(msg_channel_t))
		{
			perror("message is too small");
		}
		if(raddr.sin_port != serveraddr.sin_port || raddr.sin_addr.s_addr != serveraddr.sin_addr.s_addr)
		{
			fprintf(stderr,"Ignore: address is no match");
			continue;
		}
		if(msg_channel->chnid == chosenid)
		{
			if(writen(pd[1], msg_channel->data, packlen - sizeof(chnid_t) ) < 0)
			{
				return 0;
			}
		}



	}

	free(msg_channel);
	close(sockfd);

	if(setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
		perror("setsockopt():");
		exit(1);
	}

	return 0;

}


