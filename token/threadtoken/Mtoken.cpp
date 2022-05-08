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
#include"Mtoken.h"



#define MMAXGROUP 128
struct MGroupBase;

bool AlarmExit 	= false;
bool MGroupExit = false;
struct sigaction sigoldaction;
struct MGroupBase *baseptr;

struct MGroupBase
{
public:
	friend MDATA(Mtoken);
	friend class Mtoken;
public:
	MGroupBase()
	{
		baseptr = this;
		for(int i = 0; i < MMAXGROUP; ++i)
		{
			this->MtokGroup[i] = nullptr;
		}

		this->Mtoklock 	= PTHREAD_MUTEX_INITALIZER;
		int maxgroup	= MMAXGROUP;

		pthread_condattr_init(&(this->condattr));
		pthread_cond_init(&(this->Mtokcond), &(this->condattr));
		
		sigset_t sigmask;
		sigemptyset(&sigmask);
		struct sigaction act;
	       	act.sa_handler		= EndPro;
		act.sa_mask		= sigmask;
		act.flag		= 0;
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
	~MGroupBase()
	{
		for(int i = 0; i< MMAXGROUP; ++i)
		{
			if(this->MtokGroup[i] != nullptr)
				delete (this->MtokGroup)[i];
		}
		
		
		pthread_cond_destroy(&(this->Mtokcond))
		pthread_condattr_destroy(&(this->condattr));
		pthread_mutex_destroy(&(this->Mtoklock));
	}
public:
	Mtoken* MtokGroup[MMAXGROUP];
	pthread_mutex_t Mtoklock;//互斥量
	pthread_cond_t Mtokcond;//条件变量
	pthread_condattr_t condattr;//设置条件变量的属性
	struct timespec abstime;//设置条件变量的延时
} MainMtokGroup;

MDATA(Mtoken)
{
public:
	friend class Mtoken;
	friend struct MGroupBase;
public:
	MtokenMdata(int puttokens, int sec = 1, int pos = -1)
	{
		this->tokens 		= puttokens;
		this->Maxtokens 	= puttokens;
		this->Vaildtokens	= 0;
	       	this->pos		= pos;
		this->sec		= sec;

		sigemptyset(&(this->sigalarmset));
		sigaddset(&(this->sigalarmset), SIGALRM);	
	}
	static void* threadtoken(void *token)
	{
		MDATA(Mtoken)* Mtokptr = (MDATA(Mtoken)*)token;
		pthread_mutex_lock(&(MiantokGroup.Mtoklock));
		while(1)
	 	{
			
			pthread_cond_timedwait(&(MainMtokGroup.Mtokcond), &(MainMtokGroup.Mtoklock),
					MainMtokGroup.setWaitClock());
			//此处的返回值为abstime的地址，这里需要在系统时间上再加上一秒以等待，
			//是按照超过abstime + 等待时间（设立目标时间）来确定是否结束等待的
			sigwait(&(this->sigalarmset), SIGALRM);
			if(Mtokptr->Maxtokens > 0)
			{
				Mtokptr->Vaildtokens	+= 1;
				Mtokptr->tokens		-= 1;
			}	
			pthread_mutexlock(&(MainMtokGroup.Mtoklock));
			
		}
		pthread_exit(NULL);	
	}

	~MtokenMdata()
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
	
	pthread_t pth;
	sigset_t sigalarmset;
};

Mtoken::Mtoken();
Mtoken::Mtoken(int s, int token);
Mtoken::~Mtoken();
bool Mtoken::TokenStart();
bool Mtoken::TokenEnd();
int  Mtoken::getVailTokens();
int  Mtoken::pushTokens(int tokens);
int  Mtoken::destoryToken();
	
