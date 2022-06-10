#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<iostream>
#include<string>
#include<string.h>
#include<cstring>
#include<queue>
//#include<concepts>
#include<pthread.h>
#include<vector>
#include<thread>
#include<errno.h>
#include<string>
#include<sys/time.h>
#include<time.h>
#include"relayer.h"
#include<poll.h>
#include<sys/select.h>
#include<sys/epoll.h>
#define BUFFSIZE 1024

enum
{
	STATE_R = 0x00000001UL,
	STATE_W,
	STATE_AUTO,
	STATE_Ex,
	STATE_T
};

static MRJobGroupMdata *baseptr 	= nullptr;
static pthread_mutex_t GroupLock 	= PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t PosLock 		= PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t JobLock		= PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t ModuleLoad	= PTHREAD_ONCE_INIT;
//一次执行函数，表示无论调用pthread_once多少次，对同一个函数而言都只会执行一次

struct RelFsmStat
{	//状态机的状态
	u_int64_t state;
	int sfd;
	int dfd;
	char buff[BUFFSIZE];
	int len;
	std::string errstr;
	int pos;
	u_int64_t count;
	MDATA(MRelayer) *parent;
};

struct RelJobStat
{	//每一个作业的状态
	u_int64_t job_stat;
	int fd1, fd2;
	int oldfd1, oldfd2;
	int rd;
	struct RelFsmStat* fsm12, *fsm21;
	struct timeval startT, endT;
	bool ishasp:1;
	bool isthread:1;
};

MDATA(MRelayer)
{
public:
	MRelayerMdata(MRelayer* parent, int sfd, int dfd/*, int pos*/)
	{
		this->fstat.sfd 	= sfd;
		this->fstat.dfd 	= dfd;
		this->fstat.len 	= 0;
		this->fstat.pos 	= 0;
		//pos 这个值是用来防止write操作没有写完，而记录的buff位置
		this->fstat.state	= STATE_R;
		this->fstat.count 	= 0;
		this->fstat.parent 	= this;
		this->fstat.errstr 	= "";
		this->parent		= parent;
		memset(this->fstat.buff, '\0', BUFFSIZE);	
	}
	void setState(u_int64_t state)
	{
		this->fstat.state = state;
	}
	void FsmDriver()
	{//有限状态机编程
		int ret = 0;
		struct RelFsmStat &stat = (this->fstat);
		switch(stat.state)
		{
		case STATE_R:
			{	
				stat.len = read(stat.sfd, stat.buff, BUFFSIZE);
				if(stat.len == 0)
					stat.state = STATE_T;
				else if(stat.len < 0)
					if(errno == EAGAIN)
					//这代表当前读操作被阻塞了
						stat.state 	= STATE_R;
					else
					{	
						stat.errstr 	= std::string("read() ") + strerror(errno);
						stat.state 	= STATE_Ex;
					}
				else
				{
					stat.pos	= 0;
					stat.state 	= STATE_W;
				}
				break;
			}
		case STATE_W:
			{
				ret = write(stat.dfd, stat.buff + stat.pos, stat.len);
				if(ret < 0)
					if(errno == EAGAIN)
					{	
						stat.state 	= STATE_W;
					}
					else
						stat.state 	= STATE_Ex;
				else
				{
					stat.len 	-= ret;
					stat.pos 	+= ret;
					if(stat.len == 0)
						stat.state 	= STATE_R;
					else
						stat.state 	= STATE_W;	
				}
				break;
			}	
		case STATE_Ex:
			{
				perror(stat.errstr.c_str());
				throw stat.errstr;
				stat.state = STATE_T;
				break;
			}
		case STATE_T:
			{
				
				break;
			}
		default:
			{
				abort();
				break;
			}
		}
	}
public:
	struct RelFsmStat fstat;
	MRelayer *parent;
};

MRelayer::MRelayer(int sfd,int dfd)
{
	this->d = new MDATA(MRelayer)(this, sfd, dfd);
}
MRelayer::~MRelayer()
{
	delete this->d;
}

	
	

MDATA(MRJobGroup)
{
public:
	MRJobGroupMdata(MRJobGroup* parent)
	{	
		this->parent = parent;
		pthread_condattr_t cond_attr;
		pthread_condattr_init(&cond_attr);
		pthread_cond_init(&this->AddJobCond, &cond_attr);
		pthread_condattr_destroy(&cond_attr);
		for(auto& x : this->RJob)
		{
			x = nullptr;
		}
	}
	~MRJobGroupMdata()
	{
		this->ModuleUnload();
		for(auto& x : this->RJob)
		{
			if(x != nullptr)
			{
				if(x->ishasp == 1)
				{
					delete x->fsm12;
					delete x->fsm21;
				}
				delete x;
				x = nullptr;
			}
		}
	}
public:
	struct JobThreadId
	{
		JobThreadId(int rd, std::thread thrd)
		{
			//mcout("thread create");
			this->thrd 	= std::move(thrd);
			this->rd   	= rd;

			mcout(this->thrd.get_id());
			mcout("job init");
		}
		int rd;
		std::thread thrd;
	};
#ifdef SELECT
	struct selectfdid
	{
		selectfdid()
		{
			this->work = 0;
		}
		int work:1;
		fd_set frset;
		fd_set fwset;
	};
	using fdsetId = struct selectfdid;
	struct selectfdid* fdsetsC[JOBSMAX];//frset and fwset in class
	static void cleanDriverJobsThread(void* jobclass)
	{
		MRJobGroupMdata *job = (MRJobGroupMdata*)(jobclass);
		for(auto& x : job->fdsetsC)
		{
			delete x;
			x = nullptr;
		}
	}	

#elif defined POLL
	struct pollfdid
	{
		pollfdid()
		{
			this->work = 0;
		}
		struct pollfd pfds[2];
		int work:1;
	};
	using fdsetId = struct pollfdid;

	MRJobGroupMdata::pollfdid* pfdsc[JOBSMAX];//pfd in class
	static void cleanDriverJobsThread(void *jobclass)
	{
		MRJobGroupMdata *job = (MRJobGroupMdata*)(jobclass);
		pthread_mutex_unlock(&JobLock);
		for(auto& x : job->pfdsc)
		{
			delete x;
			x = nullptr;
		}

	}
#elif defined EPOLL
	static	void cleanDriverJobsThread(void *jobclass)
	{
		pthread_mutex_lock(&JobLock);
	}
#endif

	static void* driverJobs(void* p)
	{

		MRJobGroupMdata *rjob = (MRJobGroupMdata*)(p);
		pthread_mutex_lock(&JobLock);
							
		#ifdef SELECT
			//static fd_set frset, fwset;
			MRJobGroupMdata::selectfdid** fdsets = rjob->fdsetsC;
			for(int i = 0; i < JOBSMAX; ++i)
			{
				fdsets[i] = new MRJobGroupMdata::selectfdid();
			}
			pthread_cleanup_push(cleanDriverJobsThread, rjob);
		#elif defined POLL
			MRJobGroupMdata::pollfdid** pfds = rjob-> pfdsc;
			for(int i = 0; i < JOBSMAX; ++i)
			{
				pfds[i] = new MRJobGroupMdata::pollfdid();
			}
			pthread_cleanup_push(cleanDriverJobsThread, rjob);
		#elif defined EPOLL
			pthread_cleanup_push(cleanDriverJobsThread, rjob);
		#endif

		while(1)
		{
			
			for(int i = 0; i < JOBSMAX; ++i)
			{	

			#ifdef SELECT
				int selectsid = 0;
				for(; selectsid < JOBSMAX; ++selectsid)
				{
					if(fdsets[selectsid]->work == 0)
					{
						fdsets[selectsid]->work = 1;
						break;
					}
				}
			#elif defined POLL
				int pfdsid = 0;
				//找一个空位置来使用pollfd
				for(; pfdsid < JOBSMAX; ++pfdsid)
				{
					if(pfds[pfdsid]->work == 0 )
					{
						pfds[pfdsid]->work = 1;
						break;
					}
				}
			#elif defined EPOLL
				int epfd = epoll_create(10);
				
				struct epoll_event aev;//all epoll_data
				aev.events = 0;
				aev.data.fd = rjob->RJob[i]->fd1;
				epoll_ctl(epfd, EPOLL_CTL_ADD, rjob->RJob[i]->fd1, &aev);

				aev.events = 0;
				aev.data.fd = rjob->RJob[i]->fd2;
				epoll_ctl(epfd, EPOLL_CTL_ADD, rjob->RJob[i]->fd2, &aev);
			#endif
				if(rjob->RJob[i] != nullptr && rjob->RJob[i]->isthread == 0)
				{
					rjob->RJob[i]->isthread = 1;
					
				#ifdef SELECT
					rjob->JobsThread.push_back(JobThreadId(i, 
								std::thread([&rjob, &fdsets, i, selectsid]{  
				#elif defined POLL
					rjob->JobsThread.push_back(JobThreadId(i,
								std::thread([&rjob, &pfds, i, pfdsid]{
				#elif defined EPOLL
					mcout(rjob);
					mcout(epfd);
					mcout(i);
					rjob->JobsThread.push_back(JobThreadId(i, 
								std::thread([epfd, i]{
					//lamdba表达式以值传递传入的值是只读的	
				#endif
					MRJobGroupMdata *rjob = baseptr;
					mcout(rjob);	
					struct RelJobStat& job = *rjob->RJob[i];
					mcout("thread create");
					while(1){
					if(rjob->RJob[i]->job_stat == STATE_RUNNING )
					{
					#ifdef SELECT
						
						fd_set& frset = fdsets[selectsid]->frset; 
						fd_set& fwset = fdsets[selectsid]->fwset;	
						//布置监视任务
						FD_ZERO(&frset);
						FD_ZERO(&fwset);
						
						if(job.fsm12->state == STATE_R)
							FD_SET(job.fsm12->sfd, &frset);
						else if(job.fsm12->state == STATE_W)
							FD_SET(job.fsm12->dfd, &fwset);
						if(job.fsm21->state == STATE_R)
							FD_SET(job.fsm21->sfd, &frset);
						else if(job.fsm21->state == STATE_W)
							FD_SET(job.fsm21->dfd, &fwset);
						//存在问题，对异常态和结束态没有解决
						//解决：增加一个auto，将后两个状态无条件推动
						

						//监视	
						if(job.fsm12->state < STATE_AUTO 
								|| job.fsm21->state < STATE_AUTO)
						{
							if(select(((job.fd1 > job.fd2)?job.fd1:job.fd2)+1, 
										&frset, &fwset, NULL, NULL) < 0)
							{
								//select返回的是你所感兴趣的事件发生的文件描述符
								//返回0代表超时，返回-1代表有错误发生
								//这个函数的缺点在于输入的读写集合会被修改，
								//如果发生假错在这里就只能够重新布置监视任务
								if(errno == EINTR)
								{
									continue;
								}
								else
								{
									perror("select");
									throw std::string(strerror(errno));
									abort();
									break;
								}
							}
						}
					#elif defined POLL
						//布置监视任务
						struct pollfd* pfdptr = pfds[pfdsid]->pfds;
						pfdptr[0].fd 	= job.fd1;
						pfdptr[0].events	= 0;
						pfdptr[1].fd	= job.fd2;
						pfdptr[1].events 	= 0;
						if(job.fsm12->state == STATE_R)
							pfdptr[0].events |= POLLIN;
						else if(job.fsm12->state == STATE_W)
							pfdptr[0].events |= POLLOUT;
						if(job.fsm21->state == STATE_R)
							pfdptr[1].events |= POLLIN;
						else if(job.fsm21->state == STATE_W)
							pfdptr[1].events |= POLLOUT;
						

						//监视
						if(job.fsm12->state < STATE_AUTO 
								|| job.fsm21->state < STATE_AUTO)
						{
							while((poll(pfdptr, 2, -1)) < 0)
							{
							//poll相比于select的好处是，写入事件和返回事件分开存放
							//所以不需要去绕大圈，一个小循环可以解决问题，
							//不用重新布置监视任务
								
								if(errno == EINTR)
									continue;
								else
								{
									perror("polll() ");
									throw std::string(strerror(errno));
									abort();
									break;
								}
							
							}
						}

					#elif defined EPOLL
						mcout("监视任务布置");	
						struct epoll_event ev;
						//memcpy(&ev, &aev, sizeof(struct epoll_event));	
						ev.data.fd 	= job.fd1;
						ev.events 	= 0;	
						if(job.fsm12->state == STATE_R)
							ev.events |= EPOLLIN;
						else if(job.fsm12->state == STATE_W)
							ev.events |= EPOLLOUT;

						epoll_ctl(epfd, EPOLL_CTL_MOD, job.fd1, &ev);
						
						ev.data.fd 	= job.fd2;
						ev.events 	= 0;	
						if(job.fsm21->state == STATE_R)
							ev.events |= EPOLLIN;
						else if(job.fsm21->state == STATE_W)
							ev.events |= EPOLLOUT;
						
						epoll_ctl(epfd, EPOLL_CTL_MOD, job.fd2, &ev);

						if(job.fsm12->state < STATE_AUTO 
								|| job.fsm21->state < STATE_AUTO)
						{
							while(epoll_wait(epfd, &ev, 1, -1) < 0)
							{
							//epoll相比于select的好处是，写入事件和返回事件分开存放
							//所以不需要去绕大圈，一个小循环可以解决问题，
							//不用重新布置监视任务
							
							//epoll和poll相比，其管理数组有内核管理

								if(errno == EINTR)
									continue;
								else
								{
									perror("epoll() ");
									throw std::string(strerror(errno));
									abort();
									break;
								}
							
							}
						}


					#endif


						//查看监视结果
					#ifdef SELECT
						//（当1可读或者2可写）或反之时，推动状态机
						if(FD_ISSET(job.fd1, &frset) || FD_ISSET(job.fd2, &fwset)
								||job.fsm12->state > STATE_AUTO 
								||job.fsm21->state > STATE_AUTO)
							job.fsm12->parent->FsmDriver();
						if(FD_ISSET(job.fd2, &frset) || FD_ISSET(job.fd1, &fwset)
								||job.fsm12->state > STATE_AUTO
								||job.fsm21->state > STATE_AUTO)
							job.fsm21->parent->FsmDriver();
					#elif defined POLL
						if(pfdptr[0].revents & POLLIN 
								||pfdptr[1].revents & POLLOUT
								||job.fsm12->state > STATE_AUTO 
								||job.fsm21->state > STATE_AUTO)
							job.fsm12->parent->FsmDriver();
						if(pfdptr[1].revents & POLLIN 
								||pfdptr[0].revents & POLLOUT
								||job.fsm12->state > STATE_AUTO
								||job.fsm21->state > STATE_AUTO)
							job.fsm21->parent->FsmDriver();

					#elif defined EPOLL
						if((ev.data.fd == job.fd1 
									&& ev.events & EPOLLIN) 
								|| (ev.data.fd == job.fd2
									&& ev.events & EPOLLOUT)
								||job.fsm12->state > STATE_AUTO 
								||job.fsm21->state > STATE_AUTO)
							job.fsm12->parent->FsmDriver();
						if((ev.data.fd == job.fd2 
									&& ev.events & EPOLLIN)
								||(ev.data.fd == job.fd1 && 
									ev.events & EPOLLOUT)
								||job.fsm12->state > STATE_AUTO
								||job.fsm21->state > STATE_AUTO)
							job.fsm21->parent->FsmDriver();


					#endif	

						if(job.fsm12->state == STATE_T &&
						   job.fsm21->state == STATE_T )
						{//如果作业中的两个状态机都为结束态
							job.job_stat = STATE_OVER;
						}
					}
					}
					})));
				}
			}
	//		pthread_mutex_unlock(&JobLock);
		
			pthread_cond_wait(&rjob->AddJobCond, &JobLock);
		}
		for(auto& x : rjob->JobsThread)
		{
			if(x.thrd.joinable())
				x.thrd.join();
			else
				mcout("thread cannot join");
		}
	
	
		pthread_cleanup_pop(0);
		//当参数为0时，为本线程调用pthread_exit或者其他线程调用取消函数的时候qizuoyong
	
	
		return NULL;	
	}
	static void ModuleLoad(void)
	{
		int err = pthread_create(&baseptr->driverJob, NULL, driverJobs, baseptr);
		if(err)
		{
			char *p = new char[100];
			snprintf(p, 100, "pthread_create(): %s", strerror(err));
			fprintf(stderr, "%s", p);
			throw std::string(p);
			delete p;
			errno = err;
			return;
		}
	}
	void ModuleUnload(void)
	{
		for(auto& x : this->JobsThread)
		{
			pthread_cancel(x.thrd.native_handle());
			x.thrd.join();
		}
		pthread_cond_destroy(&this->AddJobCond);
		pthread_cancel(this->driverJob);
		pthread_join(this->driverJob, NULL);
	}
public:
	struct RelJobStat* RJob[JOBSMAX];
	std::vector<MRJobGroupMdata::JobThreadId> JobsThread;
	pthread_t driverJob;
	pthread_cond_t AddJobCond;
	MRJobGroup *parent;
};

MRJobGroup::MRJobGroup()
{
	int err = 0;
	if(err =  pthread_mutex_trylock(&GroupLock))
	{
		this->d = nullptr;
		errno 	= EBUSY;
		throw std::string("can only exit one jobgroup");
		return;
	}
	this->d = new MDATA(MRJobGroup)(this);

	baseptr = this->d;
}
MRJobGroup::~MRJobGroup()
{
	delete this->d;
	pthread_mutex_unlock(&GroupLock);
	pthread_mutex_destroy(&GroupLock);
}

int MRJobGroup::MRelAddJob(int fd1, int fd2)
{
	
	MRelayer* Relay1 = new MRelayer(fd1,fd2);
	MRelayer* Relay2 = new MRelayer(fd2,fd1); 
	int rd = this->MRelAddJob(Relay1, Relay2);
	if(rd < 0)
	{
		delete Relay1;
		delete Relay2;
		return rd;
	}
	this->d->RJob[rd]->ishasp = 1;
	return rd;
}
/*
*      return  rd successed    
*              -EIVNAL error arg 
*              -ENOSPC full of jobs
*              -ENOMEM no memory
* */
int MRJobGroup::MRelAddJob(MRelayer* sjob, MRelayer* djob)
{
	//这里存在对这个模块的操作，可能会有多个任务同时添加
	//所以需要加锁处理
	mcout("addjob work");
	pthread_mutex_lock(&PosLock);
	pthread_once(&ModuleLoad, this->d->ModuleLoad);	
	if( sjob == nullptr || djob == nullptr)
	{
		errno = EINVAL;
		throw std::string("err argument");
		pthread_mutex_unlock(&PosLock);
		return -EINVAL;
	}
	int rd = 0;
	for(; rd < JOBSMAX; ++rd)
	{
		if(this->d->RJob[rd] == nullptr)
		{
			break;
		}
	}
	if(rd == JOBSMAX)
	{
		errno = ENOSPC;
		throw std::string("full of jobs");
		pthread_mutex_unlock(&PosLock);
		return -ENOSPC;
	}
	
	

	this->d->RJob[rd] 	= new  struct RelJobStat;
	struct RelJobStat *job 	= this->d->RJob[rd];
	pthread_mutex_unlock(&PosLock);
	
	job->job_stat		= STATE_RUNNING;
       	job->fd1		= sjob->d->fstat.sfd;
	job->fd2		= sjob->d->fstat.dfd;
	job->oldfd1		= fcntl(job->fd1, F_GETFL);
	job->oldfd2		= fcntl(job->fd2, F_GETFL);
	//这里是先获取当前的状态,以便后续恢复
	job->fsm12		= &sjob->d->fstat;
	job->fsm21		= &djob->d->fstat;
	job->rd			= rd;
	job->ishasp		= 0;
	job->isthread		= 0;
	gettimeofday(&job->startT, NULL);
	gettimeofday(&job->endT, NULL);


        //这里需要对文件描述符来设置成非阻塞的状态
        fcntl(job->fd1, F_SETFL, job->oldfd1 | O_NONBLOCK);
        fcntl(job->fd2, F_SETFL, job->oldfd2 | O_NONBLOCK);
	pthread_cond_broadcast(&this->d->AddJobCond);
	return rd;
}

int MRJobGroup::MRelCanelJob(int rd)
{
        if(rd > JOBSMAX || rd < 0)
        {
                errno = EINVAL;
                throw std::string("out of range");
                return -EINVAL;
        }
        if(this->d->RJob[rd] == nullptr )
        {
                errno = EINVAL;
                throw std::string("it hasn't job");
                return -EINVAL;
        }

	this->d->RJob[rd]->job_stat = STATE_CANCEL;
	return 0;
}
/*
*      return  0 successed
*              -EBUSY jobs already be caneled
*              -EINVAL error arg
* */
int MRJobGroup::MRelWaitJob(int rd, struct MRelStatSt* jobstat)
{
	if(jobstat != nullptr)
	{
		int err = this->MRelJobStat(rd, jobstat);
		jobstat->state = STATE_OVER;
		if(err != 0)
		{
			return -errno; 
		}
	}
	struct RelJobStat *job 	= this->d->RJob[rd];
	for(auto& x : this->d->JobsThread)
	{
		if(x.rd == rd)
		{
	
			pthread_cancel(x.thrd.native_handle());
			x.thrd.join();
			break;
		}
	}

	fcntl(job->fd1, F_SETFL, job->oldfd1);
	fcntl(job->fd2, F_SETFL, job->oldfd2);


	if(job->ishasp == 1)
	{
		delete job->fsm12;
		delete job->fsm21;
	}
	delete job;
	job 		 	= nullptr;
	this->d->RJob[rd] 	= nullptr;

	return 0;
}
/*
*      return  0 successed
*              -EINVAL err arg
* */
int MRJobGroup::MRelJobStat(int rd, struct MRelStatSt* Jobstat)
{
	struct MRelStatSt tmp = this->operator[](rd);
	if(tmp.isvaild == 0)
	{
		return -errno;
	}
	
	memcpy(Jobstat, &tmp, sizeof(struct MRelStatSt));
	return 0;
}

struct MRelStatSt MRJobGroup::operator[](int rd)
{
	struct MRelStatSt rt;
	if(rd > JOBSMAX || rd < 0)
	{
		rt.isvaild = 0;
		errno = EINVAL;		
		throw std::string("out of range");
		return rt;
	}
	if(this->d->RJob[rd] == nullptr )
	{
		rt.isvaild = 0;
		errno = EINVAL;
		throw std::string("it hasn't job");
		return rt;
	}
	
	struct RelJobStat* job	= this->d->RJob[rd];
	rt.state 		= job->job_stat;
        rt.fd1 			= job->fd1;
        rt.fd2 			= job->fd2;
	rt.cout12 		= job->fsm12->count;
	rt.cout21 		= job->fsm21->count;
	rt.startT 		= job->startT;
	rt.endT 		= job->startT;
	rt.isvaild		= 1;
	return rt;
}

