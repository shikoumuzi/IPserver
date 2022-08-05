#include<stdio.h>
#include<iostream>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/socket.h>
#include<pthread.h>
#include<getopt.h>
#include<syslog.h>
#include<ipv4serverproto.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<string.h>
#include<signal.h>
#include<netinet/in.h>
#include<netinet/udp.h>

#include"medialib.h"
#include"thr_channel.h"
#include"thr_list.h"
#include"server_conf.h"
#include"server.h"

namespace MUZI{

MPRIVATE(Server)
{
public:
	ServerMPrivate()
	{
		this->sockres 	= new sockres;
		sockres->SockInit();
		this->medialib 	= MUZI::MediaLib::getMediaLib();
		this->medialist = MUZI::ThrMediaList::getThrMediaList(this->sockres);
		this->channel 	= MUZI::ThrMediaChannel::getThrMediaChannel(*this->medialib, this->scokres);	
	}
	~ServerMPrivate()
	{
		delete this->channel;
		delete this->medialist;
		delete this->medialib;
		delete this->sockres;
	}
public:
	static deamon_exit()
	{
			
	}
	int deamonize()
	{
		this->pid = fork();

		if(this->pid < 0)
		{
			//当使用守护进程时，通过写系统日志完成
			//故使用syslog
			//perror("fork(): ")
			syslog(LOG_ERR, "fork(): %s", strerror(errno));
			return -1;
		}

		if(this->pid > 0)
		{
			exit(0);
		}
		//输出错误信息
		int fd = open("/dev/null", O_RDWR);
		if(fd < 0)
		{
		//      perror("open(): ");
			syslog(LOG_WARNING, "open(): %s", strerror(errno));
			return -errno;
		}
		else
		{
			dup2(fd, 0);
			dup2(fd, 1);
			dup2(fd, 2);

			if(fd > 2)
			{
				close(fd);
			}

			setsid();

			chdir(".");
			//更改工作路径
			umask(0);
			return 0;
		}
	}

public:
	pid_t pid;
	ThrMediaChannel* channel;
	ThrMediaList* medialist;
	MediaLib *medialib;
	SockRes *sockres;
	std::vector<std::string> duffformat;
	mlib_listentry_t *chnllist;
	int listsize;
};

Server::Server(int argc, char* argv[]):d(new MPRIVATE(Server))
{
	this->analyseOpt(argc, argv);

	openlog("netradio_cpp", LOG_PIS | LOG_PERROR, LOG_DAEMON);
	switch(serverconf.runmod)
        {
        case RUN_DEAMON:
                {
                        int dearet = 0;
                        if((dearet = this->d->deamonize()) < 0)
                        {
                                perror("fork():");
                                return -errno;
                        }
                        break;
                }
        case RUN_FOREGROUND:
                {
                        /*do something*/
                        break;
                }
        default:
                {
                        errno = EINVAL;
                        syslog(LOG_ERR, "main(): %s", strerror(errno));
                        break;
                }
        }

}
Server::~Server();
int Server::SigInit(int signum[])
{
	struct sigaction sigact;
	sigact.sa_handler = this->deamon_exit();	
	sigact.sa_flags   = 0;
	sigemptyset(&sigact.sa_mask);
	for(int* p = signum; p != nullptr; ++p)
	{
		sigaddset(&sigact.sa_mask, *p);
		sigaction(*p, &sigact, NULL);
	}

}
int Server::analyseOpt(int argc, char* argv[])
{

       while(1)
        {
                int c = getopt(argc, argv, "M:P:FD:I:H");
                if(c < 0)
                {
                        break;
                }
                switch(c)
                {
                case 'M':{
                               serverconf.mgroup        = optarg;
                               break;
                       }
                case 'P':{
                               serverconf.rcvport       = optarg;
                               break;
                       }
                case 'F':{
                               serverconf.runmod        = RUN_FOREGROUND;
                               break;
                       }
                case 'D':{
                               serverconf.media_dir     = optarg;
                               break;
                       }
                case 'I':{
                               serverconf.ifname        = optarg;
                               break;
                       }
                case 'H':{
                               ServerHelpPrintf();
                               break;
                       }
                default:
                       abort();
                       break;
                }
        }
	
	
}

int Server::assignFormat(std::vector<std::string>& duffforamt);
int Server::createListThr()
{}
int Server::createChnlThr()
{}

};
