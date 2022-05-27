#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<iostream>
#include<string>
#include<string.h>
#include<queue>
//#include<concepts>
#include<pthread.h>
#include<errno.h>
#include<string>
#include<sys/time.h>
#include<time.h>
#include"relayer.h"
#define BUFFSIZE 1024

enum
{
	STATE_R = 0x00000001UL,
	STATE_W,
	STATE_AUTO,
	STATE_Ex,
	STATE_T
};
MRJobGroup *baseptr = nullptr;
pthread_mutex_t GroupLock = PTHREAD_MUTEX_INITIALIZER;

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
};

MDATA(MRelayer)
{
public:
	MRelayerMdata(MRelayer* parent, int sfd, int dfd/*, int pos*/)
	{
		this->fstat.sfd 	= sfd;
		this->fstat.dfd 	= dfd;
		this->fstat.len 	= 0;
	//	this->fstat.pos 	= pos;
		this->fstat.count 	= 0;
		this->fstat.parent 	= this;
		this->fstat.errstr 	= "";
		this->parent		= parent;
		memset(this->fstat.buff, '\0', BUFFSIZE);	
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
		for(auto& x : this->RJob)
		{
			x = nullptr;
		}
	}
	~MRJobGroupMdata()
	{
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
	struct RelJobStat* RJob[JOBSMAX];
	MRJobGroup *parent;
};

MRJobGroup::MRJobGroup()
{
	int err = 0;
	if(err =  pthread_mutex_trylock(&GroupLock))
	{
		errno = EBUSY;
		throw std::string("can only exit one jobgroup");
		return;
	}
	this->d = new MDATA(MRJobGroup)(this);
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
	if(rd == -EINVAL || rd == -ENOSPC)
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
	
	if( sjob == nullptr || djob == nullptr)
	{
		errno = EINVAL;
		throw std::string("err argument");
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
		return -ENOSPC;
	}
	
	this->d->RJob[rd] 	= new  struct RelJobStat;
	struct RelJobStat *job 	= this->d->RJob[rd];
	
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
	gettimeofday(&job->startT, NULL);
	gettimeofday(&job->endT, NULL);


        //这里需要对文件描述符来设置成非阻塞的状态
        fcntl(job->fd1, F_SETFL, job->oldfd1 | O_NONBLOCK);
        fcntl(job->fd2, F_SETFL, job->oldfd2 | O_NONBLOCK);
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
	int err = this->MRelJobStat(rd, jobstat);
	jobstat->state = STATE_OVER;
	if(err != 0)
	{
		return -errno; 
	}

	struct RelJobStat *job 	= this->d->RJob[rd];
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

