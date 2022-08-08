#include<sys/types.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<getopt.h>
#include<glob.h>
#include<errno.h>

#include<list>

#include"client.h"
#include<ipv4serverproto.h>
namespace MUZI{

MPRIVATE(Client):public MIpv4ServerProto
{
private:
	struct ChnlPack
	{
	public:
		ChnlPack(msg_channel_t* data, u_int64_t packlen)
		{
			this->data = data;
			this->packlen = packlen;
		}
		~ChnlPack()
		{
			free(this->data);
		}
	public:
		msg_channel_t* data;
		u_int64_t packlen;
	};
public:
	ClientMPrivate(Client* client)
	{
		this->msg_list	 	= (msg_list_t*)malloc(MSG_LIST_MAX);
		this->msg_channel 	= (msg_channel_t*)malloc(MSG_CHANNEL_MAX);
		this->listchoose	= 0;
		this->sockfd		= 0;
		this->packlen		= 0;
		this->parent		= client;
		pthread_mutex_init(&this->wdlock, NULL);
		pthread_cond_init(&this->wdcond, NULL);
	}
	~ClientMPrivate()
	{
		free(this->msg_list);

		pthread_cancel(this->ptid_wd);
		pthread_cancel(this->ptid_rcv);
		pthread_join(this->ptid_wd, NULL);
		pthread_join(this->ptid_rcv, NULL);
		
		
		pthread_cond_destroy(&this->wdcond);
		pthread_mutex_destroy(&this->wdlock);	
		for(auto& x : this->channelbuff)
		{
			if(x != nullptr)
			{
				delete x;
				x = nullptr;
			}
		}
		close(this->sockfd);

		if(setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
					&this->mreqn, sizeof(this->mreqn)) < 0)
		{
			perror("setsockopt():");
			exit(1);
		}

	}
	int rcvMediaList()
	{
		if(this->msg_list == NULL)
		{
			perror("msg_list: malloc():");
			exit(1);
		}
		socklen_t serveraddrlen = sizeof(this->serveraddr);
		while(1)
		{
			this->packlen = 0;
			if ((this->packlen = 
				recvfrom(this->sockfd, 
					msg_list, 
					MSG_LIST_MAX, 
					0,
			       		(struct sockaddr*)&this->serveraddr, 
					&serveraddrlen) ) < sizeof(msg_list_t))
			{
				fprintf(stderr, "message is too small.\n");
				continue;
			}
			if(this->msg_list->chnid != LISTCHNID )
			{
				fprintf(stderr, "chnid is no match.\n");
				continue;
			}
			break;
		}

		//打印节目单
		msg_listentry_t *pos = msg_list->entry;
		for(; (char*)pos < ((char*)msg_list + this->packlen);
				pos = (msg_listentry_t*)(((char*)pos) + ntohs(pos->len)))
		//通过强转char*保证指针运算一个字节一个字节相加
		//由于这里是变长结构体套变长结构体，所以需要其中一个len数据长度来确定偏移量
		{
			printf("channel: %3d : %s\n", pos->chnid, pos->desc);
		}

		//选择频道
		std::cin>> this->listchoose;	
	}
	struct timespec *getWaitTime()
	{

	}
	static void* rcvPack(void*p)
	{
		MPRIVATE(Client) *data = (MPRIVATE(Client)*)p;
		msg_channel_t* msg_channel = (msg_channel_t*)malloc(MSG_CHANNEL_MAX);
		if(msg_channel == NULL)
		{
			perror("msg_channel: mallock():");
			exit(1);
		}
		socklen_t raddrlen = sizeof(data->rcvaddr);
		
		int wdflag = 2;
		while(1)
		{
			int packlen = 0;
			if(packlen = recvfrom(data->sockfd, msg_channel, MSG_CHANNEL_MAX, 0,
					(struct sockaddr*)&data->rcvaddr, &raddrlen) < sizeof(msg_channel_t))
			{
				perror("message is too small");
			}
			if(data->rcvaddr.sin_port != data->serveraddr.sin_port 
			|| data->rcvaddr.sin_addr.s_addr != data->serveraddr.sin_addr.s_addr)
			{
				fprintf(stderr,"Ignore: address is no match");
				continue;
			}
			if(msg_channel->chnid == data->listchoose)
			{
				if(data->channelbuff.size() <= CHANNEL_BUFF * wdflag)
				{
					wdflag = 1;
				}
				else
				{
					pthread_cond_broadcast(&data->wdcond);
				}
				
				data->channelbuff.push_back(new ChnlPack(msg_channel, packlen));
			}



        	}

	}
	static void* sndPack(void *p)
	{
		MPRIVATE(Client)* data = (MPRIVATE(Client)*)p;
		ChnlPack* package = nullptr; 
		while(1)
		{
			package = data->channelbuff.front();
			if(data->parent->writen(package->data->data,
		       		package->packlen - sizeof(chnid_t) - sizeof(u_int64_t)) < 0)
			{
			
			}
			else
			{
				delete package;
				package = nullptr;
				data->channelbuff.erase(data->channelbuff.begin());
			}
		}
	}
public:
	int pd[2];
	pthread_t ptid_rcv;
	pthread_t ptid_wd;
	pthread_mutex_t wdlock;
	pthread_cond_t wdcond;
	pid_t pid;
	int sockfd;
	struct ip_mreqn mreqn;
	struct sockaddr_in locaddr, rcvaddr, serveraddr;
	msg_list_t *msg_list;
	msg_channel_t *msg_channel;
	int listchoose;
	size_t packlen;
	std::list<struct ChnlPack*> channelbuff;
	Client* parent;
};





Client::Client(int argc, char* argv[])
{
	this->analyseOpt(argc, argv);
	this->d = new MPRIVATE(Client)(this);
}
Client::~Client()
{
	delete this->d;
}
int Client::analyseOpt(int argc, char* argv[])
{
	int c = 0, index = 0;
	struct option cmdopt[] = {
			{"mgroup", '1', NULL , 'M'},
			{"port",'1',NULL, 'P'},
			{"player",'1',NULL,'P'},
			{"help",'0',NULL,'H'},
			{NULL, 0, NULL, 0}};
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
			 return -1;
		}

	}

	return 0;
}
int Client::SockInit()
{
	this->d->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(this->d->sockfd < 0)
	{
		perror("socket():");
		exit(1);
	}

	struct ip_mreqn mreq;
	inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
	inet_pton(AF_INET, client_conf.mgroup, &mreq.imr_multiaddr);
	mreq.imr_ifindex = if_nametoindex("eth0");
	
	if(setsockopt(this->d->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
				&this->d->mreqn, sizeof(this->d->mreqn)) < 0)
	{
		perror("setsockopt(): IP_ADDMEMEBERSHIP: ");
		exit(1);
	}
	
	bool val = 1;
	if(setsockopt(this->d->sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val)) < 0)
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

	if(bind(this->d->sockfd, (struct sockaddr*)&laddr, sizeof(laddr) ) < 0)
	{
		perror("bind():");
		exit(1);
	}

	return 0;
}
int Client::create()
{
	if(pipe(this->d->pd) < 0)
	{
		perror("pipe() : ");
		exit(1);
	}
	
	this->d->pid = fork();

	
	//子进程：调用解码器
	//父进程：从网络上收包，发送给子进程
	if(this->d->pid < 0)
	{
		perror("fork():");
		exit(1);
	}

	if(this->d->pid == 0)
	{
		close(this->d->sockfd);
		close(this->d->pd[1]);
		close(0);
		dup2(this->d->pd[0], 0);
		if(this->d->pd[0] > 0)
		{
			close(this->d->pd[0]);
		}	
		
		//可以通过shell来执行播放器命令
		execl("/bin/sh", "sh", "-c", client_conf.player_cmd, NULL);
		perror("execl");
		exit(1);
	}
	
	close(this->d->pd[0]);
	return 0;
}
int Client::writen(char *buff, int buffsize)
{
        int len = buffsize, ret = 0, pos = 0;
        while(len > 0)
        {
                ret = write(this->d->pd[1], buff + pos, buffsize);
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

	return len;
}
int Client::start()
{
	pthread_create(&this->d->ptid_rcv, NULL, this->d->rcvPack, this->d);
	pthread_create(&this->d->ptid_wd, NULL, this->d->sndPack, this->d);
	return 0;
}
};
