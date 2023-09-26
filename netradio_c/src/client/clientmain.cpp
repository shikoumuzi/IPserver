#include<queue>
#include<sys/types.h>
#include<sys/stat.h>
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
#include<thread>
#include<mutex>
#include<string.h>
#include<fcntl.h>
#include<signal.h>


/*
 *-M --mgroup: 指定多播组
 *-P --port: 指定接收端口
 *-p --player: 指定解码器
 *-H --help: 帮助
*/

static int work_state_output_pipe_fd = 0;
static int cmd_pipe_fd = 0;
static int ret_pipe_fd = 0;
static int mplayer_cmd_pipe_fd = 0;
pid_t mplayer_pid = 0;

static void signal_handler(int signum)
{
	printf("close all fd and child process");
	//close(cmd_pipe_fd);
	//close(ret_pipe_fd);
	close(mplayer_cmd_pipe_fd);
	close(work_state_output_pipe_fd);
	kill(mplayer_pid, SIGINT);
	kill(mplayer_pid, SIGTERM);
	char cmd[48];
	sprintf(cmd, "kill -9 %d", mplayer_pid);
	system(cmd);	
	exit(0);
}


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

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	
	printf("msg:[signal handler binding]\n");

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
	printf("msg:[socket binding]\n");	

	if(pipe(pd) < 0)
	{
		perror("pipe() : ");
		exit(1);
	}
	
	printf("msg:[unnamed pipe created]\n");
	if(access(client_conf.mplayer_cmd_pipe_file_path, F_OK) < 0)
	{
		fprintf(stdout, "msg:[named pipe created]\n");
		mkfifo(client_conf.mplayer_cmd_pipe_file_path, 0777);
	}
	printf("msg:[mplayer_cmd_pipe created]\n");

//	if(access(client_conf.client_cmd_pip_file_path, F_OK) < 0)
//	{
//		fprintf(stdout, "msg:[named pipe created]\n");
//		mkfifo(client_conf.client_cmd_pip_file_path, 0766);
//	}
//	
//	printf("msg:[client_cmd_pipe created]\n");
//
//	if(access(client_conf.client_ret_pip_file_path, F_OK) < 0)
//	{
//		fprintf(stdout, "msg:[named pipe created]\n");
//		mkfifo(client_conf.client_ret_pip_file_path, 0766);
//	}
//	
//	printf("msg:[mplayer_ret_pipe created]\n");


	if(access(client_conf.client_work_state_output_pipe_file_path, F_OK) < 0)
	{
		fprintf(stdout, "msg:[named pipe created]\n");
		mkfifo(client_conf.client_work_state_output_pipe_file_path, 0777);
	}
	
	printf("msg:[mplayer_work_state_output_pipe created]\n");



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
	
	printf("msg:[mplayer process created]\n");
	
	close(pd[0]);
	
	printf("msg:[start to create pipe communcation]\n");
	
	int mplayer_cmd_writer_fd = open(client_conf.mplayer_cmd_pipe_file_path, O_WRONLY);
	if(mplayer_cmd_writer_fd < 0)
	{
		perror("error:[mplayer cmd pipe open error]");
		exit(1);
	}
	printf("msg:[mplayer_cmd_writer_fd created]\n");
	
//	int cmd_read_fd = open(client_conf.client_cmd_pip_file_path, O_RDONLY);
//	if(cmd_read_fd < 0)
//	{
//		perror("error:[cmd pipe open error]");
//		exit(1);
//	}
//	printf("msg:[cmd_read_fd created]\n");
//
//	int ret_writer_fd = open(client_conf.client_ret_pip_file_path, O_WRONLY);
//	if(ret_writer_fd < 0)
//	{
//		perror("error:[ret pipe open error]");
//		exit(1);	
//	}
//	printf("msg:[ret_Writer_fd created]\n");

	
	int work_state_output_writer_fd = open(client_conf.client_work_state_output_pipe_file_path, O_WRONLY);
	if(work_state_output_writer_fd < 0)
	{
		perror("error:[ret pipe open error]");
		exit(1);	
	}
	printf("msg:[work_state_output_writer_fd created]\n");
	


	//cmd_pipe_fd = cmd_read_fd;
	//ret_pipe_fd = ret_writer_fd;
	mplayer_cmd_pipe_fd = mplayer_cmd_writer_fd;
	work_state_output_pipe_fd = work_state_output_writer_fd;


//	std::thread analyzed_cmd_thread(
//			[&pd, &cmd_read_fd, &mplayer_cmd_writer_fd, &ret_writer_fd]()
//			{
//				const char* mplayer_pause = "pause\n";
//				const char* mplayer_stop = "stop\n";
//				const char* mplayer_player = "";
//				const char* ret_success = "success\n";
//				const char* ret_error = "error\n";
//				bool pause_now = false;
//
//				char buf[8] = {'\0'};
//				while(true)
//				{
//					memset(buf, '\0', 8);
//
//					ssize_t size = read(cmd_read_fd, buf, 8);
//					if(size > 0)
//					{
//						printf("msg:[c-cmd:%s]", buf);
//						switch(buf[0])
//						{
//						case 'P': // 暂停
//						
//							if(!pause_now)
//							{
//								write(mplayer_cmd_writer_fd, 
//									mplayer_pause, 
//									sizeof(mplayer_pause));
//								
//								write(ret_pipe_fd,
//									ret_success,
//									sizeof(ret_success));
//								pause_now = true;
//							}
//							break;
//						case 'S': // 停止
//								
//							if(write(mplayer_cmd_writer_fd, 
//								mplayer_stop, 
//								sizeof(mplayer_stop)) > 0)
//							{	
//								write(mplayer_cmd_writer_fd, 
//									mplayer_stop, 
//									sizeof(ret_success));
//								
//								write(ret_pipe_fd,
//									ret_success,
//									sizeof(ret_success));
//								
//								kill(0, SIGINT);
//							}
//							break;
//						case 'A': // 播放
//							
//							if(pause_now)
//							{
//								write(mplayer_cmd_writer_fd, 
//									mplayer_pause, 
//									sizeof(mplayer_pause));
//								
//								write(ret_pipe_fd,
//									ret_success,
//									sizeof(ret_success));
//								pause_now = false;	
//							}
//							break;
//						case 'F': // 快进 
//							break;
//						default:
//							write(ret_pipe_fd, ret_error, sizeof(ret_error));
//							break;
//						}
//
//					}		
//				}
//			});
//	
//	
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
		
		char buf[48] = {'\0'};
		int buf_size = sprintf(buf, "work:[packlen=%d]\n", packlen);
		write(work_state_output_pipe_fd, buf, buf_size);	
		
		if(msg_list->chnid != LISTCHNID )
		{
			fprintf(stderr, "chnid is no match.\n");
			continue;
		}
		break;
	}	

	//打印节目单
	msg_listentry_t *pos = msg_list->entry;
	for(; (char*)pos < ((char*)msg_list + packlen) ; 
			pos = (msg_listentry_t*)(((char*)pos) + ntohs(pos->len)))
	//通过强转char*保证指针运算一个字节一个字节相加
	//由于这里是变长结构体套变长结构体，所以需要其中一个len数据长度来确定偏移量
	{
		printf("channel: %3ld : %s\n", pos->chnid, pos->desc);
	}

	//选择频道
	int chosenid = 0;
//	while(1)
//	{
		printf("选择频道: ");
//		int ret = scanf("%d", &chosenid);
		std::cin >> chosenid;
//		if(ret != 1)
//		{
//			perror("input can not scanf");
//			exit(1);
//		}
		printf("你已经选择了第 %d 频道", chosenid);
//		break;
//	}
	
	std::queue<msg_channel_detail_t> msg_buff;
	std::mutex lock;


	//收频道包发给子进程
	msg_channel_t* msg_channel = (msg_channel_t*)malloc(MSG_CHANNEL_MAX);
	if(msg_channel == NULL)
	{
		perror("msg_channel: mallock():");
		exit(1);
	}
	
	packlen = 0;	
	printf("revfrom(): ready");
	bool clean_buff = false;
	bool buff_completed = false;

		
	std::thread msg_writer(
			[&lock, &msg_buff, &buff_completed, &pd]()
			{
				int write_len = 0;
				while(1)
				{
				        if(msg_buff.size() < 16)
					{
						sleep(1);
					//	printf("buff now");
						continue;
					}
					// printf("check buff\n");

					buff_completed = true;
					lock.lock();
					
					msg_channel_detail_t front_msg;

					if(msg_buff.size() > 1)
					{
						front_msg = msg_buff.front();
						msg_buff.pop();
					}
					else
					{
						lock.unlock();
						continue;
					}

					lock.unlock();
						
					char write_pipe_state[48];
				       	int work_pipe_len = sprintf(write_pipe_state, "msg:[write to mplayer]\n");
					write(work_state_output_pipe_fd, write_pipe_state, work_pipe_len);
					
					if((write_len = write(pd[1], front_msg.data->data, front_msg.size - sizeof(chnid_t) )) < 0)
					{
						fprintf(stderr, "writen error errno is %d, error message is %s", errno, strerror(errno));
						printf("error:[error message is %s]", strerror(errno));
						//perror(strerror(errno));
						return 0;
					}

					memset(write_pipe_state, '\0', work_pipe_len);
					work_pipe_len = sprintf(write_pipe_state, "msg:[writed len=%d]\n", write_len);

					// work_pipe_len = sprintf(write_pipe_state, "msg:[size=%d]\n", front_msg.size - sizeof(chnid_t));
					write(work_state_output_pipe_fd, write_pipe_state, work_pipe_len);
					if(errno == EAGAIN || errno == EIO)
					{
						sleep(1);
					}

					
					if(write_len != front_msg.size - sizeof(chnid_t))
					{
						sleep(1);
					}

					sleep(1);
					usleep(500000);
					free(front_msg.data);

				}
			});

	while(1)
	{
		if((packlen = recvfrom(sockfd, msg_channel, MSG_CHANNEL_MAX, 0,
				(struct sockaddr*)&raddr, &raddrlen)) < sizeof(msg_channel_t))
		{
			perror("message is too small");
		}
		else
		{
			char buf[48] = {'\0'};
			int buf_size = sprintf(buf, "work:[packlen=%d]\n", packlen);
			write(work_state_output_pipe_fd, buf, buf_size);	
		}
		if(raddr.sin_port != serveraddr.sin_port || raddr.sin_addr.s_addr != serveraddr.sin_addr.s_addr)
		{
			fprintf(stderr,"Ignore: address is no match");
			continue;
		}
		
		if(msg_channel->chnid == chosenid)
		{
			// if(! buff_completed)
			char buf[48] = {'\0'};
		       	int buf_size = sprintf(buf, "work:[now_buff_size=%d]\n", msg_buff.size());	
			//printf(buf);
			write(work_state_output_pipe_fd, buf, buf_size);	
			
			lock.lock();
			msg_buff.push({.data=msg_channel,
					.size=packlen});
                        msg_channel = (msg_channel_t*)malloc(MSG_CHANNEL_MAX);
                        if(msg_buff.size() > 2048)
                        {
                                msg_channel_detail_t front_msg = msg_buff.front();
                                free(front_msg.data);
                                msg_buff.pop();
                        }
			lock.unlock();
			// printf("push over\n");
			// printf("recv one package\n");
//                        else if(msg_buff.size() < 64)
//                        {
//				printf("buff now");
//				continue;
//                        }
//
//			buff_completed = true;
//                        msg_channel_t* front_msg = msg_buff.front();
//                        msg_buff.pop();
//		
//			if(writen(pd[1], front_msg->data, packlen - sizeof(chnid_t) ) < 0)
//			{
//				return 0;
//			}
//			sleep(1);
//			free(front_msg);
		}



	}

	free(msg_channel);
	close(sockfd);
	close(cmd_pipe_fd);
	close(ret_pipe_fd);
	close(mplayer_cmd_writer_fd);

	if(setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
		perror("setsockopt():");
		exit(1);
	}

	return 0;

}


