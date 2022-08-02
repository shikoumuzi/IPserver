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

static int deamonize()
{
	pid_t pid = fork();
	
	if(pid < 0)
	{
		//当使用守护进程时，通过写系统日志完成
		//故使用syslog
		//perror("fork(): ")
		syslog(LOG_ERR, "fork(): %s", strerror(errno));
		return -1;
	}
	

	if(pid > 0)
	{
		exit(0);
	}
	//输出错误信息
	int fd = open("/dev/null", O_RDWR);
	if(fd < 0)
	{
	//	perror("open(): ");
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
		umask(0);
		return 0;
	}

}

int serversd = 0;
static int socket_init()
{
	if((serversd =  socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		syslog(LOG_ERR, "socket_init():socket(): %s , socksd: %d" ,strerror(errno),serversd);
		exit(1);
	}

	struct ip_mreqn mreq;
	inet_pton(AF_INET, serverconf.mgroup, &mreq.imr_multiaddr);
	inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
	mreq.imr_ifindex = if_nametoindex(serverconf.ifname);


	if(setsockopt(serversd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0)
	{
		syslog(LOG_ERR, "setsockopt(): IPPROTO_IP %s", strerror(errno));
	}

	sndaddr.sin_family 	= AF_INET;
	sndaddr.sin_port	= htons(atoi(serverconf.rcvport));
	inet_pton(AF_INET, serverconf.mgroup, &sndaddr.sin_addr);
	return 0;
}


static mlib_listentry_t *chnlist;
static void deamon_exit(int s)
{
	thr_list_destroy();
	thr_channel_destroy_all();
	mlib_freechnlist(chnlist);
	close(serversd);
	closelog();

	exit(0);
}

static void errexit(int s)
{
	exit(1);
}

server_conf_t serverconf = {
			.rcvport 	= DEFAULT_RCVPORT,
			.mgroup	 	= DEFAULT_MGROUP,
			.media_dir	= DEFAULT_MEDIADIR,
			.ifname		= DEFAULT_IF,
			.runmod		= RUN_DEAMON};


int main(int argc, char* argv[])
{
	//守护进程写系统日志
	openlog("netradio", LOG_PID | LOG_PERROR, LOG_DAEMON);
	//指定身份， 指定写日志权限

	
	struct sigaction sigact;
	sigact.sa_handler 	= deamon_exit;
	sigact.sa_flags	= 0;
	sigemptyset(&sigact.sa_mask);
	sigaddset(&sigact.sa_mask, SIGTERM);
	sigaddset(&sigact.sa_mask, SIGINT);
	sigaddset(&sigact.sa_mask, SIGQUIT);


	sigaction(SIGTERM, &sigact, NULL);//用来打断一个行为
	sigaction(SIGINT,  &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	sigemptyset(&sigact.sa_mask);
	sigact.sa_handler	= errexit;
	sigaddset(&sigact.sa_mask, SIGKILL);
	sigaction(SIGKILL, &sigact, NULL);
//

	/*命令行分析*/
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
			       serverconf.mgroup	= optarg;
			       break;
		       }
		case 'P':{
			       serverconf.rcvport	= optarg;
			       break;
		       }
		case 'F':{
			       serverconf.runmod	= RUN_FOREGROUND;
			       break;
		       }
		case 'D':{
			       serverconf.media_dir	= optarg;
			       break;
		       }
		case 'I':{
			       serverconf.ifname	= optarg;
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


	/*守护进程实现*/
	/*当实现守护进程时，如果需要看输出，需要重定向一个文件到标准输出上*/
	switch(serverconf.runmod)
	{
	case RUN_DEAMON: 
		{
	//		int dearet = 0;
	//		if((dearet = deamonize()) < 0)
	//		{
	//			perror("fork():");
	//			return -errno;	
	//		}
	//		break;
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

	/*SOCKET初始化*/
	socket_init();	
	/*获取频道信息*/
	int list_size = 0, listerr = 0;

	int err = 0;	

	if((err = mlib_getchnlist(&chnlist, &list_size)) > 0)
	{
		syslog(LOG_ERR, "mlib_getchnlist():%s", strerror(err));
		exit(1);
	}
	/*创建节目单线程*/
	if((err = thr_list_create(chnlist, list_size)))
	{
		exit(1);
	}

	/*延迟抖动： 延迟的时间有长有短，造成收包混乱*/


	/*创建频道线程*/
	int i = 0;
	for(; i < list_size; ++i)
	{
		err = thr_channel_create(chnlist + i);
		if(err)
		{
			syslog(LOG_WARNING,"thr_channel_create(): %s",strerror(err));
			exit(1);
		}
	}	

	
	syslog(LOG_DEBUG, "%d channel thread created %d", i, list_size);


	while(1)
		pause();


	closelog();	

}


