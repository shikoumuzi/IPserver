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

MPRIVATE(Server) *dataptr = nullptr;
MPRIVATE(Server)
{
public:
	ServerMPrivate(Server* parent)
	{
		this->parent	= parent;
		this->sockres 	= new SockRes;
		sockres->SockInit();
		this->medialib 	= MUZI::MediaLib::getMediaLib();
		this->medialist = MUZI::ThrMediaList::getThrMediaList(this->sockres);
		this->channel 	= MUZI::ThrMediaChannel::getThrMediaChannel(*this->medialib, this->sockres);
	}
	~ServerMPrivate()
	{
		delete this->channel;
		
		delete this->medialist;
		
		delete this->medialib;
		
		delete this->sockres;
		
	}
public:
	static void deamon_exit(int signum)
	{
		dataptr->parent->~Server();	
	}
	static void ServerHelpPrintf()
	{
		std::cout
			<<"*-M --mgroup		指定多播组"
			<<"*-P --port 		指定端口"
			<<"*-F --front 		前台运行"
			<<"*-D --		指定媒体库"
			<<"*-I --interface	指定网络设备"
			<<"*-H --help		查看帮助"<<std::endl;
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
	int getChannelList()
	{
		return this->medialib->getchnlist(&this->chnllist, 
					&this->listsize, this->duffformat);
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
	Server* parent;
};

Server::Server(int argc, char* argv[])
{	
	if(dataptr != nullptr)
	{
		return ;
	}
	this->analyseOpt(argc, argv);

	this->d = new MPRIVATE(Server)(this);
	dataptr = this->d;


	openlog("netradio_cpp", LOG_PID | LOG_PERROR, LOG_DAEMON);
	switch(serverconf.runmod)
        {
        case RUN_DEAMON:
                {
//                        int dearet = 0;
//                        if((dearet = this->d->deamonize()) < 0)
//                        {
//                                perror("fork():");
//				syslog(LOG_ERR, "Server::Server(): fork() failed. err: %s",
//						strerror(errno));	
//				return;
//			}
//                        break;
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
Server::~Server()
{
	delete this->d;
	closelog();
}
int Server::SigInit(std::vector<int>& signum)
{
	struct sigaction sigact;
	sigact.sa_handler = this->d->deamon_exit;	
	sigact.sa_flags   = 0;
	sigemptyset(&sigact.sa_mask);
	for(auto& x : signum)
	{
		sigaddset(&sigact.sa_mask, x);
		sigaction(x, &sigact, NULL);
	}

	return 0;
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
                               this->d->ServerHelpPrintf();
                               break;
                       }
                default:
                       abort();
                       return -1;
		}
        }
	
	return 0;	
}

int Server::assignFormat(std::vector<std::string>& duffformat)
{
	for(int i = 0; i < duffformat.size(); ++i)
		this->d->duffformat.push_back(duffformat[i]);
	return 0;
}
int Server::getChnlList()
{
	if(this->d->getChannelList() < 0)
	{
		syslog(LOG_ERR, 
			"Server::getChannelList(): get channel list form media failed. err:%s",
			strerror(errno));
		return -1;
	}
	return 0;
}

int Server::createListThr()
{
	if(this->d->medialist->create(this->d->chnllist, this->d->listsize) < 0)
	{

		syslog(LOG_ERR, 
			"Server::createListThr(): channel list thread create failed. err: %s",
			strerror(errno));
		return -1;
	}
	return 0;
}
int Server::createChnlThr()
{
	for(int i = 0; i < this->d->listsize; ++i)
	{
		if(this->d->channel->create(this->d->chnllist + i) < 0)
		{
			syslog(LOG_ERR,
				"Server::createChnlThr(): channel thread create failed: err:%s",
				strerror(errno));
			return -1;
		}
	}
	return 0;
}

};
