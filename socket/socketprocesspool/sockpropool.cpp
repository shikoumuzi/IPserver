#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<pthread.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
//#include<string>
#include<string.h>
#include<cstring>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<sys/msg.h>
//#include<sys/mman.h>
#include<error.h>
#include<atomic>
#include"sockpropool.h"
#include"../../ftb/msg/ftb_msg.h"

MSockPro* onlySockptr = nullptr;

MSockPro* MSockProInit(std::string ipstr,
			void(*datactl)(void*),
                        u_int64_t net,
                        int inetproto,
                        int port,
                        int minspareserver,
                        int maxspareserver,
                        int maxclient)
{
        MSockPro* ret =  (MSockPro*)mmap(NULL,sizeof(MSockPro),PROT_READ|PROT_WRITE,
                           MAP_SHARED| MAP_ANONYMOUS,-1,0);
        ret->MSockProInit(ipstr, datactl, net, inetproto, port, minspareserver, maxspareserver, maxclient);
        return ret;
}
/*template<typename T>
void mcout(T data)
{
	std::cout<<data<<std::endl;
}*/

struct server_st
{
	pid_t pid;
	u_int64_t state;
	int reuse;
	int pos;
	int client_sd;
};

MPRIVATE(MSockPro)
{
public:
	static void user2_handler(int signum)
	{
		return;
	}
public:
	MSockProMPrivate():mmsg()
	{}
	void MSockProMPrivateInit(MSockPro * parent,
				std::string ipstr,
				void(*datactl)(void*),
				int net,
				int inetproto,	
				int port, 
				int minspareserver,
				int maxspareserver,
				int maxclients)
	{
			
		
			struct sigaction sa, osa;
		       	sa.sa_handler 		= SIG_IGN;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags		= SA_NOCLDWAIT;
			//阻止子进程变成僵尸态，即自动收尸	
			sigaction(SIGCHLD, &sa, &osa);
			
			sa.sa_handler		= user2_handler;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags		= 0;
			sigaction(SIG_NOTIFY, &sa, &osa);
			//父进程收尸所用到的信号就是SIG_CHID，在这里需要先阻塞住
			
			sigset_t sigset;
			sigemptyset(&sigset);
			sigaddset(&sigset, SIG_NOTIFY);
			sigprocmask(SIG_BLOCK, &sigset, &this->oldset);
		
			pthread_condattr_t condattr;
			pthread_condattr_init(&condattr);
			pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);	
			pthread_cond_init(&this->procond, &condattr);
			pthread_condattr_destroy(&condattr);
			
			pthread_mutexattr_t mutexattr;
			pthread_mutexattr_init(&mutexattr);
			pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
			pthread_mutex_init(&this->promutex, &mutexattr);
			pthread_mutex_destroy(&this->promutex);

			//mcout("signale readly");
			MMsg* tmpmsg		= new MMsg;
			//mcout("new");
			this->mmsg		= (MMsg*)
                                                        mmap(NULL,
                                                        sizeof(MMsg),
                                                        PROT_READ|PROT_WRITE,
                                                        MAP_SHARED| MAP_ANONYMOUS,
                                                        -1,0);
			//mcout("mmap");
			(*this->mmsg)		= std::move(*tmpmsg);
			delete tmpmsg;
			//mcout("cpy");
			//mcout("delete");
			this->parent		= parent;
			this->datactl		= datactl;
			this->ipstr		= ipstr;
			this->net		=  (net != MINET && net != MINET6)
							?(fprintf(stderr, "It is invaild arg in net"),
							       exit(1),0)
							:(net);

			this->port		= port;
			this->inetproto		= (inetproto != MTCP && inetproto != MUDP)
							?(fprintf(stderr, "It is invaild arg in inetproto"),
							       exit(1),0)
							:(inetproto);
			this->minspareserver	= minspareserver;
			this->maxspareserver	= maxspareserver;
			this->maxclients	= (maxclients < maxspareserver)
							? maxspareserver : maxclients;
			this->sd		= socket((this->net == MINET)
									? AF_INET : AF_INET6,
								(this->inetproto == MTCP)
									? SOCK_STREAM :SOCK_DGRAM,0);
			if(this->sd == -1)
			{
				perror("MSockPro()::socket():");
				exit(1);
			}
			//mcout(this->sd);
			this->serverpool	= (struct server_st**)
							mmap(NULL, 
							sizeof(struct server_st*)*this->maxclients,
							PROT_READ|PROT_WRITE, 
							MAP_SHARED| MAP_ANONYMOUS,
							-1,0);
			//内存映射函数	
			//mcout(this->serverpool);	
			if(this->serverpool == MAP_FAILED)
			{
				perror("MSockPro()::mmap():");
				exit(1);
			}
			for(int i = 0; i < this->maxclients; ++i)
			{
				this->serverpool[i] = nullptr;
			}
			int val 		= 1;
			if(setsockopt(this->sd,SOL_SOCKET, SO_REUSEADDR /*| SO_REUSERPORT*/, &val,sizeof(val))< 0)
			{
				perror("MSockPro()::setsockopt():");
				exit(1);
			}	
			struct sockaddr_in laddr, raddr;
			struct sockaddr_in6 laddr6, raddr6;
			if(this->inetproto == MINET)
			{
				laddr.sin_family 	= AF_INET;
				laddr.sin_port		= htons(this->port);
			        inet_pton(AF_INET, this->ipstr.c_str(), &laddr.sin_addr);	
				if(bind(this->sd, (struct sockaddr*)&laddr, sizeof(laddr)) < 0)
				{
					perror("MSockPro()::bind():");
					exit(1);
				}
			}
			else
			{
				
			}
			//mcout("work");
			if(listen(this->sd, 3*this->maxclients) < 0)
			{
				perror("MSockPro()::listen():");
				exit(1);
			}
			this->idle_count 	= 0;	
			this->busy_count	= 0;
			this->pronum		= 1;
			//mcout("work");		
			this-> smd		= this->mmsg->MMsgRegister(MMSG_PUBLIC | MMSG_SEND, SOCKPOOLPATHNAME);
			//mcout("smd    ============");
			//mcout(this->smd);
			this->rmd		= this->mmsg->MMsgRegister(MMSG_PUBLIC | MMSG_RCV, SOCKPOOLPATHNAME);
			//mcout("rmd     ===========");
			//mcout(this->smd);
			if(smd < 0 || rmd < 0)
			{
				perror("sockpropool init failed");
				this->~MSockProMPrivate();
				return;
			}
			//sleep(10);			
	}
	void server_job(int pos)
	{
		int ppid = getppid();
		int& client_sd = this->serverpool[pos]->client_sd;
		struct sockaddr raddr;
		//time_t stamp = time(NULL);
		//struct tm *p = gmtime(&stamp);
		char buff[1024];
		int bufflen = 1024, len = 0;
		//这里加个消息队列来获取要发送或者接受的数据
		while(1)
		{
			//mcout("working()");
			this->serverpool[pos]->state = MSTATE_IDLE;
			kill(ppid, SIG_NOTIFY);
			//通知父进程前来查看
			socklen_t raddrlen = sizeof(raddr);
			//mcout("accept():working");
			client_sd = accept(this->sd,(struct sockaddr*)&raddr, &raddrlen);
			if(client_sd < 0)
			{
				if(errno == EINTR || errno == EAGAIN)
				{
					continue;
				}
				else
				{
					perror("accept():");	
					exit(1);
				}
			}
			this->serverpool[pos]->state = MSTATE_BUSY;
			kill(ppid, SIG_NOTIFY);
			/*len = snprintf(buff, bufflen, "%d:%d:%d:%d:%d:%d\n",
				       	p->tm_year, p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min,p->tm_sec);
			*/
			struct data_format data;
			memset(data.data, '\0', MAXDATASPACE);
			pthread_cond_broadcast(&this->procond);

			this->parent->MRecvData(&data, sizeof(data)); 
			//mcout("RCVdata");
			//mcout(data.data);	
			//send(client_sd, sendpack.c_str(), sendpack.size(), 0);	
			send(client_sd, &data, sizeof(data.data), 0);
			//send(client_sd, buff, len, 0);
			sleep(1);		
			close(client_sd);
			//stamp = time(NULL);
			//p = gmtime(&stamp);
		}
	}	
	int AddServer(int num)
	{
		//mcout("addserver");
		int forknum = 0;
		if(this->idle_count + this->busy_count >= this->maxspareserver || num == 0)
			return -1;
		int solt = 0;
		for(; solt < this->maxclients; ++solt)
			if(this->serverpool[solt] == nullptr)
			{
				++this->pronum;
				++this->idle_count;
				break;
			}
		if(solt != maxclients)
		{
			this->serverpool[solt]		= (struct server_st*)
								  mmap(NULL,
								  sizeof(struct server_st),
								  PROT_READ|PROT_WRITE,
								  MAP_SHARED| MAP_ANONYMOUS,
								  -1,0);
			struct server_st* proptr	= this->serverpool[solt];
			proptr->state			= MSTATE_IDLE;
			proptr->pos			= solt;
			proptr->reuse			= 0;
			proptr->pid 			= fork();
			proptr->client_sd		= 0;
			if(proptr->pid < 0)
			{
				perror("fork():");
				errno = ENOMEM;
				return -ENOMEM;
			}
			else if(proptr->pid == 0)
			{
				//printf("fork(): finish\n");
				this->server_job(solt);
				exit(0);
			}
			else
			{
				proptr->reuse		+= 1;
				//printf("create one\n");
			}

			int ret = 0;
			++forknum += ((ret = this->AddServer(num - 1)) < 0)? 0 : ret;
		}
		return forknum;
	}
	int DelServer(int num)
	{
		//mcout("delserver");
		int delnum = 0;
		if(this->idle_count == 0 || num == 0)
		{
			return 0;
		}
			
		for(int i = 0; i < this->maxclients; ++i)
		{
			if(this->serverpool[i] != nullptr && this->serverpool[i]->state == MSTATE_IDLE)
			{
				kill(this->serverpool[i]->pid, SIGKILL);
				//mcout("munmap");		
				munmap(this->serverpool[i], sizeof(struct server_st));
				//mcout("delserver free");
				this->serverpool[i]=nullptr;
				--this->idle_count;
				--this->pronum;
				break;
			}

		}
		int ret = 0;
		
		++delnum = (((ret = this->DelServer(num - 1)) < 0)) ? 0 : ret;
		//mcout("delnum");
		return delnum;
	}
	int ScanPool()
	{
		int idle = 0, busy = 0;
		for(int i = 0; i < this->maxclients; ++i)
		{
			if(this->serverpool[i] != nullptr)
			{
				if(kill(this->serverpool[i]->pid,0))
				{
					munmap(this->serverpool[i],sizeof(struct server_st));
					this->serverpool[i]=nullptr;
					continue;
				}
				if(this->serverpool[i]->state == MSTATE_IDLE)
					idle ++;
				else if(this->serverpool[i]->state == MSTATE_BUSY)
					busy ++;
			}
		}
		this->idle_count = idle;
		this->busy_count = busy;
		return 0;
	}
	~MSockProMPrivate()
	{
		if(this->pronum <= 6)
		{
			sigprocmask(SIG_SETMASK, &this->oldset, NULL);
			for(int i = 0; i < this->maxclients; ++i)
			{
				if(this->serverpool[i] != nullptr)
				{
					kill(this->serverpool[i]->pid, SIGQUIT);
					munmap(this->serverpool[i],sizeof(struct server_st));
					this->serverpool = nullptr;
				}
			}
			close(this->sd);
			munmap(this->serverpool, sizeof(struct server_st*));
			munmap(this->mmsg, sizeof(MMsg));
			pthread_cond_destroy(&this->procond);
			pthread_mutex_destroy(&this->promutex);
			this->mmsg->~MMsg();
		}
	}
public:
	struct server_st** serverpool;
	MSockPro *parent;
	MMsg* mmsg;
	int smd, rmd;
	std::string ipstr;
	//socket数据
	int sd;
	int net;
	int inetproto;
	int port;
	//progress数据
	int idle_count;
	int busy_count;	
	int minspareserver;
	int maxspareserver;
	int maxclients;
	sigset_t oldset;
	std::atomic<int> pronum;
	struct data_format databuff;
	void (*datactl)(void *);
	pthread_cond_t procond;
	pthread_mutex_t promutex;
};

void MSockPro::MSockProWait(int s)
{
	if(s == SIGQUIT)
	{
		if(onlySockptr->d->pronum <= 6)
		{
			if(onlySockptr != nullptr)
			{
				if(onlySockptr->d != nullptr)
				{
					onlySockptr->d->~MSockProMPrivate();
					munmap(onlySockptr->d, sizeof(MPRIVATE(MSockPro)));
				}
				munmap(onlySockptr, sizeof(MSockPro));
			}
		}
	}
	if(s == SIGKILL)
	{
		return;
	}
}

void MSockPro::MSockProInit(std::string ipstr, void(*datactl)(void*), u_int64_t net, int inetproto, 
				int port, int minspareserver, int maxspareserver, int maxclient)
{
	if(onlySockptr != nullptr)
	{
		throw "it must hava only one object";
		return;
	}
	onlySockptr = this;
	this->d = (MPRIVATE(MSockPro)*)
			   mmap(NULL,
			   sizeof(MPRIVATE(MSockPro)),
			   PROT_READ|PROT_WRITE,
			   MAP_SHARED| MAP_ANONYMOUS,
			  -1,0);
	this->d->MSockProMPrivateInit(this, ipstr, datactl, net, inetproto, port, 
					minspareserver, maxspareserver, maxclient);
	//mcout("init now");
	//mcout(this->d);
	if(this->d == MAP_FAILED)
	{
		perror("MSockProInit():");
		exit(1);
	}

	struct sigaction sa;
	sa.sa_handler	= this->MSockProWait;
	sa.sa_flags	= 0;
	sigemptyset(&sa.sa_mask);
	
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGKILL, &sa, NULL);

}
int MSockPro::MDriverPool()
{
	//for(int i = 0; i < this->d->minspareserver; ++i)
        time_t stamp = time(NULL);
        struct tm *p = gmtime(&stamp);
	char buff[512];
	int bufflen = 512, len = 0;	
	this->d->AddServer(this->d->minspareserver);
	
	//std::cout<<"serverpro create"<< std::endl;
	while(1)
	{
		//mcout("driver now");
		if(sigsuspend(&this->d->oldset) != -1)
		{
			if(errno == EINTR)
				continue;
		}
		//mcout("get signaal");
		this->d->ScanPool();
		//mcout("scanpool finish");
		if(this->d->idle_count > this->d->maxspareserver)
		{
			//检查空闲进程的个数并杀死多余进程
			//for(int i = 0; i < (idle_count - this->d->maxspareserver), ++i)
			this->d->DelServer(this->d->idle_count - this->d->maxspareserver);
		}	
		else if(this->d->idle_count < this->d->minspareserver)
		{
			//mcout("idle < minspare");
			//for(int i = 0; i < (this->d->minspareserver - idle_count); ++i)
			this->d->AddServer(this->d->minspareserver - this->d->idle_count);
		}

		pthread_mutex_lock(&this->d->promutex);
		pthread_cond_wait(&this->d->procond, &this->d->promutex);
		pthread_mutex_unlock(&this->d->promutex);
		
		len = snprintf(buff, bufflen, "%d:%d:%d:%d:%d:%d",
                                        p->tm_year, p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min,p->tm_sec);
			
		strncat(buff + len, ":shikoumuzi", strlen(":shikoumuzi"));
		
		this->MSendData(buff, strlen("+shikoumuzi") + len);
	
		for(int i = 0; i < this->d->maxclients; ++i)
		{
			if(this->d->serverpool[i] != nullptr)
			{
				switch(this->d->serverpool[i]->state)
				{
				case MSTATE_IDLE:
					printf(".");
					break;
				case MSTATE_BUSY:
					printf("X");
					break;
				}
			}	
		}
		printf(" idle: %d busy: %d\n", this->d->idle_count, this->d->busy_count);
		stamp = time(NULL);
		p = gmtime(&stamp);
	}
}
int MSockPro::MSendData(const void *buff, u_int64_t size)
{
	msg_data_t<struct data_format> pack, retpack;
	//mcout("MMSGDATA:   --");
	//mcout(MMSGDATA);
	pack.mtype 		= MMSGDATA;
	memset(pack.data.data, '\0', MAXDATASPACE);
	memcpy(pack.data.data, buff, size);
		
	this->d->mmsg->MMsgCtl(this->d->smd, MMSG_SEND | MMSG_KPSTAT, 
				&pack, &retpack, size , MMSGDATA);

	return 0;
}
int MSockPro::MRecvData(void *buff, u_int64_t size)
{
	msg_data_t<struct data_format> pack, retpack;
	pack.mtype = MMSGDATA;
	memset(&pack.data, '\0', MAXDATASPACE);
        memcpy(&retpack, &pack, sizeof(pack));

	this->d->mmsg->MMsgCtl(this->d->rmd, MMSG_RCV | MMSG_KPSTAT, &pack, &retpack, 
			sizeof(retpack) - sizeof(long), MMSGDATA);
//	mcout(retpack.data.data);
	memcpy(buff, &retpack.data, size);
	return 0;
}
MSockPro::~MSockPro()
{
	munmap(this->d, sizeof(MSockPro));
}


