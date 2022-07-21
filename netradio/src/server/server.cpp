#include<stdio.h>
#include<iostream>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/socket.h>
#include<pthread>
#include<getopt>
#include<syslog.h>
#include<ipv4serverproto.h>
#include<arpa/inet.h>
#include<net/if.h>

#include<medialib.h>
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
		syslog(LOG_ERR, "fork(): %s": strerror(errno));
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
		syslog(LOG_WARINIG, "open(): %s", strerror(errno))
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
static void socket_init()
{
	int serversd = 0;
	if((serversd = socket(AF_INET, SOCK_DGRAM, 0)) < 0);
	{
		syslog(LOG_ERR, "socket(): %s" ,strerror(errno));
		exit(1);
	}

	struct ip_mreqn mreq;
	inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr);
	inet_pton(AF_INET, "0.0.0.0", &mreq._imr.address);
	mreq.imr_ifindex = if_inet_nametoindex(server_conf_ifname);


	if(setsockopt(serversd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0);
	{
		syslog(LOG_ERR, "setsockopt(): IPPROTO_IP %s", strerror(errno));
	}
}

static void deamon_exit(int s)
{
	closelog();
}


server_conf_t serverconf = {
			.rcvport 	= DEFAULT_PORT,
			.mgroup	 	= DEFAULT_MGROUP,
			.media_dir	= DEFAULT_MEDIADIR,
			.runmode	= RUN_DAEMON,
			.ifname		= DEFAULT_IF
			};


int main(int argc, char* argv[])
{
	//守护进程写系统日志
	openlog("netradio", LOG_PID | LOG_PERROR, LOG_DEAMON);
	//指定身份， 指定写日志权限

	
	struct sigaction sigaction;
	sigaction.sa_handler 	= deamon_exit;
	sigemptyset(&sigaction.sa_mask);
	sigaddset(&sigaction.sa_mask, SIGTERM);
	sigaddset(&sigaction.sa_mask, SIGINT);
	sigaddset(&sigaction.sa_mask, SIGQUIT);
	sigaddset(&sigaction.sa_mask, SIGKILL);


	sigaction(SIGTERM, &sigaction, NULL);//用来打断一个行为
	sigaction(SIGINT,  &sigaction, NULL);
	sigaction(SIGQUIT, &sigaction, NULL);
	sigaction(SIGKILL, &sigaction, NULL);


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
		case M:{
			       server_conf.mgroup	= optarg;
			       break;
		       }
		case P:{
			       server_conf.port		= optarg;
			       break;
		       }
		case F:{
			       server_conf.runmod	= RUN_FOREGROUND;
			       break;
		       }
		case D:{
			       server_conf.media_dir	= optarg;
			       break;
		       }
		case I:{
			       server_conf.ifname	= optarg;
			       break;
		       }	
		case H:{
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
	switch(server_conf.runmod)
	{
	case RUN_DEAMON: 
		{
			int dearet = 0;
			if((dearet = deamonize()) < 0)
			{
				perror("fork():");
				return -errno;	
			}
			break;
		}
	case RUN_FORREFROUND:
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
	mlib_listentry_t *chnlist;
	int list_size = 0, listerr = 0;
	

	getchnlist(&chnlist, &list_size);
	/*创建节目单线程*/
	thr_list_create(chnlist, int list_size);
	/*延迟抖动： 延迟的时间有长有短，造成收包混乱*/


	/*创建频道线程*/
	int i = 0;
	for(; i < list_size; ++i)
	{
		thr_channel_create(chnlist + i);
	}	
	syslog(LOG_DEBUG, "%d channel thread created", i);


	while(1)
		pause();


	closelog();	

}


