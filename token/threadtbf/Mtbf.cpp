#include<thread>
#include<mutex>
#include<atomic>
#include<future>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<unistd.h>
#include<sys/time.h>
#include<sys/types.h>
#include<signal.h>
#include<time.h>
#include<errno.h>
#include<string>
#include"Mtbf.h"

#define MMAXGROUP 128

struct MGroupBase;

bool AlarmExit 	= false;
bool MGroupExit = false;
struct sigaction sigoldaction;
struct MGroupBase *baseptr;
MDATA(Mtbf);

enum
{
	INIT = 0X00000001UL,
	SIGNAL,
	BROADCAST,
	WAIT,
	TIMEDWAIT,
	DESTROY
};


struct MGroupBase
{
public:
	friend MDATA(Mtbf);
	friend class Mtbf;
public:
	MGroupBase()
	{
		baseptr = this;
		for(int i = 0; i < MMAXGROUP; ++i)
		{
			this->MtbfGroup[i] = nullptr;
		}

		this->Mtbflock 		= PTHREAD_MUTEX_INITIALIZER;
		this->timeout.tv_sec 	= 1;
		this->timeout.tv_usec 	= 0;
		int maxgroup	= MMAXGROUP;

		pthread_condattr_init(&(this->condattr));
		pthread_cond_init(&(this->Mtbfcond), &(this->condattr));
		pthread_cond_init(&(this->Mtbfcond_tokenchange), &(this->condattr));	
		
		sigset_t sigmask;
		sigemptyset(&sigmask);
		struct sigaction act;
	       	act.sa_handler		= EndPro;
		act.sa_mask		= sigmask;
		act.sa_flags		= 0;
		

		//当被强制退出时，所需要做的动作

		sigaction(SIGINT, &act, &sigoldaction);
		this->setWaitClock();

		
	}
	static void EndPro(int s)
	{
		baseptr->~MGroupBase();
	}
	struct timespec* setWaitClock()
	{
		clock_gettime(CLOCK_MONOTONIC, &(this->abstime));
		//CLOCK_MONOTONIC代表着从系统开机时开始计算的时间，不受用户改变的影响
		this->abstime.tv_sec += 1;	


		return &(this->abstime);
	}
	int pushCondWork(uint64_t threadnum, bool timeout = true)
	{
		int err = 0;
		do
		{	
			switch(threadnum)
			{
			case SIGNAL:
				err = pthread_cond_signal
					(&(((timeout)?this->Mtbfcond_tokenchange:this->Mtbfcond)));break;
			case BROADCAST:
				err = pthread_cond_broadcast
					(&((timeout)?this->Mtbfcond_tokenchange:this->Mtbfcond));break;
			case WAIT:
				err = pthread_cond_wait
					(&((timeout)?this->Mtbfcond_tokenchange:this->Mtbfcond),
					 &this->Mtbflock);break;
			case TIMEDWAIT:
				err = pthread_cond_timedwait
					(&(this->Mtbfcond_tokenchange),
					 &this->Mtbflock, this->setWaitClock());break;
			case DESTROY:
				err = pthread_cond_destroy
					(&((timeout)?this->Mtbfcond_tokenchange:this->Mtbfcond));break;
			default:
				break;
			}

			if(threadnum != SIGNAL 
				&& threadnum != BROADCAST
				&& threadnum != WAIT)
			{
				if(err = EINTR)
					continue;
				else if(err = EBUSY)
				{
					pthread_cond_broadcast
						(&((timeout)?this->Mtbfcond_tokenchange:this->Mtbfcond));
					//select原本是文件描述符监控函数，但当只设置timeout时，
					//他就是很好的睡眠函数
					//其由于十分的古老，因此兼容性很强
					select(0, NULL, NULL,NULL, &this->timeout);
					continue;
				}
				else if(err = ETIMEDOUT)
				{
					return ETIMEDOUT;	
				}
				else
				{
					return -err;
				}
			}
			else
			{
				break;
			}
		}while(err != 0);
		
		return 0;
	}
	int getFreePos(Mtbf* tbf)
	{
		int i = 0;
		for( ;i < MMAXGROUP; ++i)
		{
			if(this->MtbfGroup[i] == nullptr)
			{
				this->MtbfGroup[i] = tbf;
				tbf->pushTbf((void*)(&i));
				return i;
			}
		}
		if(i == MMAXGROUP)
		{
			return -EINVAL;
		}
	}
	~MGroupBase()
	{
		for(int i = 0; i< MMAXGROUP; ++i)
		{
			if(this->MtbfGroup[i] != nullptr)
				delete (this->MtbfGroup)[i];
		}
		
		this->pushCondWork(DESTROY);
		this->pushCondWork(DESTROY, false);
		pthread_condattr_destroy(&(this->condattr));
		pthread_mutex_destroy(&(this->Mtbflock));
	}
public:

	struct timeval timeout;
	Mtbf* MtbfGroup[MMAXGROUP];
	pthread_mutex_t Mtbflock;//互斥量
	pthread_cond_t Mtbfcond;//条件变量
	pthread_cond_t Mtbfcond_tokenchange;
	pthread_condattr_t condattr;//设置条件变量的属性
	struct timespec abstime;//设置条件变量的延时
} MainMtbfGroup;

MDATA(Mtbf)
{
public:
	friend class Mtbf;
	friend struct MGroupBase;
public:
	MtbfMdata(int puttokens, int sec = 1)
	{
		this->tokens 		= puttokens;
		this->Maxtokens 	= puttokens;
		this->Vaildtokens	= 0;
		this->pos		= -1;
		this->sec		= sec;
		this->currentsec	= 0;
		this->maxsec		= sec * 100;
		this->workstat		= false;
					
		sigemptyset(&(this->sigalarmset));
		sigaddset(&(this->sigalarmset), SIGALRM);	
	}
	static void* threadtbf(void *Mtbf)
	{
		MDATA(Mtbf)* Mtbfptr = (MDATA(Mtbf)*)Mtbf;
		pthread_mutex_lock(&(MainMtbfGroup.Mtbflock));
		
	
		//设置信号以启用循环
		while(1)
	 	{
			while(!Mtbfptr->workstat)
				pthread_cond_wait(&(MainMtbfGroup.Mtbfcond), &(MainMtbfGroup.Mtbflock));
		
			pthread_cond_timedwait(&(MainMtbfGroup.Mtbfcond_tokenchange), 
					&(MainMtbfGroup.Mtbflock),MainMtbfGroup.setWaitClock());
			//此处的返回值为abstime的地址，这里需要在系统时间上再加上一秒以等待，
			//是按照超过abstime + 等待时间（设立目标时间）来确定是否结束等待的
			pthread_mutex_lock(&(MainMtbfGroup.Mtbflock));	
			if(Mtbfptr->Maxtokens > 0)
			{
				Mtbfptr->Vaildtokens	+= 1;
				Mtbfptr->tokens		-= 1;
				Mtbfptr->currentsec	+= 1;
				
				if(Mtbfptr->currentsec > Mtbfptr->maxsec + Mtbfptr->sec)
					Mtbfptr->currentsec -= Mtbfptr->maxsec;	
			}	
		}
		pthread_exit(NULL);	
	}

	~MtbfMdata()
	{
		sigdelset(&(this->sigalarmset),SIGALRM);
		pthread_cancel(this->pth);
		pthread_join(this->pth, NULL);
	}
public:
	std::atomic<int> tokens;
	std::atomic<int> Maxtokens;
	std::atomic<int> Vaildtokens;
	
	int pos;
	int sec;
	int currentsec;
	int maxsec;
	bool workstat;	

	pthread_t pth;
	sigset_t sigalarmset;
};

Mtbf::Mtbf(){}
Mtbf::Mtbf(int s, int *pos, int tokens)
{
	this->d = new MDATA(Mtbf)(tokens, s);
	if( MainMtbfGroup.getFreePos(this) == -EINVAL)
	{
		throw std::string("is considerable Mtbf");
		this->~Mtbf();
	}
	*pos = this->d->pos;
	pthread_create(&(this->d->pth), NULL, this->d->threadtbf, this);
}
Mtbf::~Mtbf()
{
	if(this->d != nullptr)
		delete this->d;
}
int Mtbf::TbfStart()
{
	this->d->workstat = true;
	int err = 0;
	MainMtbfGroup.pushCondWork( BROADCAST , false);

	return 0;	
}
void Mtbf::pushTbf(void *p)
{
	int pos = *((int*)p);
	this->d->pos = pos;
}
int Mtbf::Tbfstop()
{
	this->d->workstat = false;
}
int  Mtbf::getVailTokens(int s)
{
	int rt = 0;
	
	while(this->d->currentsec % this->d->sec == 0)
		select(0, NULL, NULL, NULL, &(MainMtbfGroup.timeout));
	
	
	pthread_mutex_lock(&MainMtbfGroup.Mtbflock);
	
	int vadtok = this->d->Vaildtokens;	
	rt = (vadtok > s)?((this->d->Vaildtokens -= s),s):((this->d->Vaildtokens = 0), vadtok);		

	pthread_mutex_unlock(&MainMtbfGroup.Mtbflock);
	return rt;

}
int  Mtbf::pushTokens(int tokens)
{
	int tok 	= this->d->tokens;
	int maxtok	= this->d->tokens;
	int rt		= 0;
	pthread_mutex_lock(&MainMtbfGroup.Mtbflock);
	this->d->tokens = (tok + tokens > maxtok)? (rt = maxtok - tok,maxtok): (rt =  tok) + tokens;
	pthread_mutex_unlock(&MainMtbfGroup.Mtbflock);	
	
	return rt;
}
int  Mtbf::destoryToken()
{
	this->~Mtbf();
}
	
