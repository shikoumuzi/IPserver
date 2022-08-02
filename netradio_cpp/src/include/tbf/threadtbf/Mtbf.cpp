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
#include<string.h>
#include<signal.h>
#include<time.h>
#include<errno.h>
#include<string>
#include<syslog.h>
#include"Mtbf.h"

#define MMAXGROUP 128

struct MGroupBase;

bool AlarmExit 			= false;
bool MGroupExit 		= false;
struct MGroupBase *baseptr 	= nullptr;
class MtbfGroup *Groupptr 	= nullptr;

struct sigaction sigoldaction;
MDATA(Mtbf);
struct PrivateFun;

pthread_mutex_t* getPMutex(MDATA(Mtbf)* tbf);
pthread_cond_t* getPCond(MDATA(Mtbf)* tbf, bool timeout);
struct PrivateFun* getFun(struct PrivateData* p);

enum
{
	INIT = 0X00000001UL,
	SIGNAL,
	BROADCAST,
	WAIT,
	TIMEDWAIT,
	DESTROY
};

struct PrivateFun
{
	struct PrivateData data;
	pthread_mutex_t* (*getMutex)(MDATA(Mtbf)* tbf);
	pthread_cond_t* (*getCond)(MDATA(Mtbf)* tbf, bool timeout);
};

template<typename T>
void Mcout(T str)
{
	std::cout<<str<<std::endl;
}

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
		
	//	this->MtbfGrouplock	= PTHREAD_MUTEX_INITIALIZER;
		this->timeout.tv_sec 	= 1;
		this->timeout.tv_usec 	= 0;
		int maxgroup	= MMAXGROUP;
		
		pthread_mutexattr_t mtexatr;
		pthread_mutexattr_init(&mtexatr);
		pthread_mutexattr_settype(&mtexatr,PTHREAD_MUTEX_FAST_NP);
		pthread_mutex_init(&this->MtbfGrouplock, &mtexatr);
	//	pthread_mutexattr_destroy(&mtexatr);

	
		sigset_t sigmask;
		sigemptyset(&sigmask);
		struct sigaction act;
	       	act.sa_handler		= EndPro;
		act.sa_mask		= sigmask;
		act.sa_flags		= 0;
		this->sigact = act;

	//	当被强制退出时，所需要做的动作
		sigaction(SIGINT, &act, &sigoldaction);
	}
	static void EndPro(int s)
	{
		Mcout("stop now");
		baseptr->~MGroupBase();
	//	如果数据生成在栈区，最好只调用析构函数
	//	exit(1);
	}
	struct timespec* setWaitClock(int ms)
	{
	//	设置条件变量的延时
		struct timespec* abstime = new struct timespec;
		clock_gettime(CLOCK_MONOTONIC, (abstime));
	//	CLOCK_MONOTONIC代表着从系统开机时开始计算的时间，不受用户改变的影响
		abstime->tv_sec += ms/1000;	
		
	//	因为有可能加上秒数后超时，需要对nsec特殊处理
		long us = abstime->tv_nsec/1000 + (ms % 1000) * 1000;//先统一转成微妙
		abstime->tv_sec += us / 1000000;
		
		us %= 1000000;
		abstime->tv_nsec += 0;//us * 1000;
		return (abstime);
	}
	pthread_condattr_t* getCAttr()
	{
		static pthread_condattr_t rt;
		pthread_condattr_init(&(rt));
		pthread_condattr_setclock(&(rt), CLOCK_MONOTONIC);
		return &rt;	
	}
	int pushCondWork(Mtbf*ptr,  uint64_t threadnum, bool timeout = true)
	{
		if(ptr == nullptr)
			return -EINVAL;
		int err = 0;
		do
		{	
			switch(threadnum)
			{
			case SIGNAL:
				err = pthread_cond_signal
					((getFun(ptr->Pdata)->getCond)(ptr->d, timeout)/*ptr->getCond(timeout)*/);break;
			case BROADCAST:
				err = pthread_cond_broadcast
					((getFun(ptr->Pdata)->getCond)(ptr->d, timeout)/*ptr->getCond(timeout)*/);break;
			case WAIT:
				err = pthread_cond_wait
					((getFun(ptr->Pdata)->getCond)(ptr->d, timeout)/*ptr->getCond(timeout)*/,
					 getPMutex(ptr->d)/*ptr->getMutex()*/);break;
			case TIMEDWAIT:
				{
				struct timespec* tmptime = this->setWaitClock(1000);
				err = pthread_cond_timedwait(
					(getFun(ptr->Pdata)->getCond)(ptr->d, true)/*ptr->getCond(true)*/,
					 (getFun(ptr->Pdata)->getMutex)(ptr->d)/*ptr->getMutex()*/, tmptime);break;
				delete (tmptime);
				}
			case DESTROY:
				err = pthread_cond_destroy(
					(getFun(ptr->Pdata)->getCond)(ptr->d, timeout)/*ptr->getCond(timeout)*/);break;
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
						((getFun(ptr->Pdata)->getCond)(ptr->d, timeout)/*ptr->getCond(timeout)*/);
				//	select原本是文件描述符监控函数，但当只设置timeout时，
				//	他就是很好的睡眠函数
				//	其由于十分的古老，因此兼容性很强
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
		return -1;
	}
	~MGroupBase()
	{
		for(int i = 0; i< MMAXGROUP; ++i)
		{
			if(this->MtbfGroup[i] != nullptr)
				(this->MtbfGroup)[i]->~Mtbf();
		}
		
		if(Groupptr != nullptr)
			delete Groupptr;
		
		pthread_mutex_destroy(&(this->MtbfGrouplock));
		pthread_condattr_destroy(&(this->condattr));
	}
public:

	struct timeval timeout;
	Mtbf* MtbfGroup[MMAXGROUP];
	pthread_mutex_t MtbfGrouplock;
	struct sigaction sigact;
	pthread_condattr_t condattr;//设置条件变量的属性
} MainMtbfGroup;

MDATA(Mtbf)
{
public:
	friend class Mtbf;
	friend struct MGroupBase;
	friend pthread_mutex_t* getPMutex(MDATA(Mtbf)* tbf);
	friend pthread_cond_t* getPCond(MDATA(Mtbf)* tbf);
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
	//	this->Mtbflock		= PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;	
	//	只有在集合声明，定义和初始化一起使用，才能够使用这种静态初始化互斥量的方法
	//	不然的话只能使用如下所示的动态函数初始化的方法 	

		pthread_mutexattr_t mtexatr;
		pthread_mutexattr_init(&mtexatr);
		pthread_mutexattr_settype(&mtexatr, PTHREAD_MUTEX_FAST_NP);
		pthread_mutex_init(&this->Mtbflock, &mtexatr);
		
		pthread_cond_init(&(this->Mtbfcond), NULL);
		pthread_cond_init(&(this->Mtbfcond_tokenchange), (MainMtbfGroup.getCAttr()));
		
		sigemptyset(&(this->sigalarmset));
		sigaddset(&(this->sigalarmset), SIGALRM);	

		pthread_mutexattr_destroy(&mtexatr);
	}
	static void* threadtbf(void *Mtbf)
	{
		MDATA(Mtbf)* Mtbfptr = (MDATA(Mtbf)*)Mtbf;
	//	std::cout << &Mtbfptr->Mtbflock<< std::endl;
	
		int err = 0;
	//	Mcout("mutex work");	
	//	设置信号以启用循环
		while(1)
	 	{

			pthread_mutex_lock(&Mtbfptr->Mtbflock);
			
		//	syslog(LOG_DEBUG,"thread id %d,while open once  %d, workstat:%s",pthread_self(), (Mtbfptr->workstat)?"true":"false");
			while(!Mtbfptr->workstat)
			{	
				err = pthread_cond_wait(&(Mtbfptr->Mtbfcond), &(Mtbfptr->Mtbflock));
			}
		//	Mcout("thread work now");
			
		//	pthread_mutex_unlock(&(Mtbfptr->Mtbflock));
		//	pthread_mutex_lock(&Mtbfptr->Mtbflock);
		//	一旦这里带入的超时时间的nsec超过1s就会返回无效参数 EINVAL
			struct timespec* tmptime = MainMtbfGroup.setWaitClock(1000);
			syslog(LOG_DEBUG,"thread id %d, pthread_cond_timedwait ready, timeout [%d:%d]", pthread_self(), tmptime->tv_sec, tmptime->tv_nsec);
			
			while(( err = pthread_cond_timedwait(&(Mtbfptr->Mtbfcond_tokenchange), 
					&(Mtbfptr->Mtbflock),
					tmptime)) != ETIMEDOUT || err == EINTR)
			{
				syslog(LOG_DEBUG, "thread id %d, tbf_thread:pthread_cond_timeout()\n", pthread_self());
		//		fprintf(stdout, "pthread_cond_timeout: %s\n", strerror(err));
		//		std::cout<< err <<std::endl;
				sleep(1);
		
		//		pthread_mutex_lock(&Mtbfptr->Mtbflock);
				delete tmptime;
				
				tmptime = MainMtbfGroup.setWaitClock(1000);
			}
			delete tmptime;
		//	Mcout("timeout");
		//	此处的返回值为abstime的地址，这里需要在系统时间上再加上一秒以等待，
		//	是按照超过abstime + 等待时间（设立目标时间）来确定是否结束等待的
			pthread_mutex_unlock(&(Mtbfptr->Mtbflock));
			//在多任务中，自行手动释放锁，来出让给其他线程调度
			if(Mtbfptr->Maxtokens > 0)
			{
				Mtbfptr->Vaildtokens	+= 1;
				Mtbfptr->tokens		-= 1;
				Mtbfptr->currentsec	+= 1;
				
				if(Mtbfptr->currentsec > Mtbfptr->maxsec + Mtbfptr->sec)
					Mtbfptr->currentsec -= Mtbfptr->maxsec;	
				int tmp = Mtbfptr->Vaildtokens;
				syslog(LOG_DEBUG,"thread id %d,vaildtoken ++, now is %d\n",pthread_self(), tmp);
			}	
	//		syslog(LOG_DEBUG,"thread id %d,while finish once\n",pthread_self());
			sched_yield();
		}
	//	pthread_exit(NULL);	
	}
	~MtbfMdata()
	{
		sigdelset(&(this->sigalarmset),SIGALRM);
		
		pthread_mutex_unlock(&this->Mtbflock);
		pthread_cond_destroy(&this->Mtbfcond);
		pthread_cond_destroy(&this->Mtbfcond_tokenchange);
		pthread_cancel(this->pth);
	//	这里需要先把其他的该销毁的销毁，最后取消线程
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


	pthread_mutex_t Mtbflock;
	pthread_cond_t Mtbfcond;
	pthread_cond_t Mtbfcond_tokenchange;	
	pthread_t pth;
	sigset_t sigalarmset;
};

pthread_mutex_t* getPMutex(MDATA(Mtbf)* tbf)
{
	return &tbf->Mtbflock;
}
//c++最好不要给类型加上括号，特别是带class或者struct的类名
pthread_cond_t* getPCond(MDATA(Mtbf)* tbf, bool timeout)
{
	return &((timeout)?tbf->Mtbfcond_tokenchange:tbf->Mtbfcond);
}

//此处为c隐藏私有变量的方法
//因为结构体本质上是对一段内存的描述，通过将第一个变量定义为一个在头文件暴露的结构体
//进而实现这个结构体和私有变量结构体的内存地址一致
//而后通过函数来强制类型转换指针，进而实现操作私有变量
//struct PrivateFun
//{
//	struct PrivateData data;
//	pthread_mutex_t* (*getMutex)(MDATA(Mtbf)* tbf);
//	pthread_cond_t* (*getCond)(MDATA(Mtbf)* tbf, bool timeout);
//};


struct PrivateFun* getFun(struct PrivateData* p)
{
	return (struct PrivateFun*)p;
}

struct PrivateData* createFun(Mtbf* tbf)
{
	struct PrivateFun* p = new (struct PrivateFun);
	p->data.data = nullptr;
	p->getMutex = getPMutex;
	p->getCond = getPCond;	
	return &(p->data);
}

pthread_mutex_t* getMutex(struct PrivateData* ptr, MDATA(Mtbf)* tbf)
{	
	struct PrivateFun* p = (struct PrivateFun*)ptr;
	return	(*(p->getMutex))(tbf);	
}
pthread_cond_t* getCond(struct PrivateData* ptr, bool timeout, MDATA(Mtbf)* tbf)
{
	struct PrivateFun* p = (struct PrivateFun*)ptr;
	return (*(p->getCond))(tbf, timeout);
}

void destoryFun(struct PrivateData* ptr)
{
	struct PrivateFun* p = (struct PrivateFun *)ptr;
	delete p;  
}

//这里结束

Mtbf::Mtbf():d(nullptr){}
Mtbf::Mtbf(int s, int *pos, int tokens)
{
	this->createTbf( s,  pos,  tokens);
}
Mtbf::~Mtbf()
{
//	destoryFun(this->Pdata);
	MainMtbfGroup.MtbfGroup[this->d->pos] = nullptr;
	if(this->d != nullptr)
		delete this->d;
}
int Mtbf::TbfStart()
{
	this->d->workstat = true;
	int err = 0;
	MainMtbfGroup.pushCondWork(this, BROADCAST , false);

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
	return 0;
}
void Mtbf::createTbf(int s, int *pos, int tokens)
{
	sigaction(SIGINT, &MainMtbfGroup.sigact, NULL);

	this->d = new MDATA(Mtbf)(tokens, s);
	if( MainMtbfGroup.getFreePos(this) == -EINVAL)
	{
		throw std::string("is considerable Mtbf");
		this->~Mtbf();
	}
	if(pos != nullptr)
		*pos = this->d->pos;
	if(Groupptr != nullptr)
	{
		Groupptr->pushTbf();
	}
//	Mcout( &(this->d->Mtbflock) );
	pthread_create(&(this->d->pth), NULL, this->d->threadtbf, (void*)(this->d));
//	这里在排查时，出现重大错误，传入的类型出错导致一直使用了未初始化的锁，导致tpp.c84位置报错
//	耗时三小时才找出bug 谨此记住该错误
	
	this->Pdata = createFun(this);

}
int  Mtbf::getVaildTokens(int s)
{             
	int rt = 0;
//	Mcout("getvaildtokens work");	
	while(!(this->d->sec != 0 && this->d->currentsec % this->d->sec <= 1 && this->d->currentsec >= this->d->sec))
		select(0, NULL, NULL, NULL, &(MainMtbfGroup.timeout));
//	Mcout("gettoken");
	
		
	int vadtok = 0;
	
	do{
		
		pthread_mutex_lock(&this->d->Mtbflock);
		//syslog(LOG_DEBUG, "thread id %d, pid: %d, getVaildTokens now", pthread_self(), this->d->pth);
	//	if(pthread_kill(this->d->pth, 0) != 0)
	//	{		
	//		syslog(LOG_DEBUG, "tbf_thread is canel by unkown questioni : %s", strerror(errno));
	//		exit(1);
	//	}
		
		vadtok = this->d->Vaildtokens;	
		rt = (vadtok > s)?((this->d->Vaildtokens -= s),s):((this->d->Vaildtokens = 0), vadtok);		
	
		//select(0, NULL, NULL, NULL, &(MainMtbfGroup.timeout));
	
	
		pthread_mutex_unlock(&this->d->Mtbflock);
		sched_yield();

	
	}while(rt == 0);
	return rt;

}
int  Mtbf::pushTokens(int tokens)
{
	int tok 	= this->d->tokens;
	int maxtok	= this->d->tokens;
	int rt		= 0;
	pthread_mutex_lock(&this->d->Mtbflock);
	this->d->tokens = (tok + tokens > maxtok)? (rt = maxtok - tok,maxtok): (rt =  tok) + tokens;
	pthread_mutex_unlock(&this->d->Mtbflock);	
	
	return rt;
}
int  Mtbf::destoryToken()
{
	this->~Mtbf();
	return 0;
}

MDATA(MtbfGroup)
{
public:
	MtbfGroupMdata()
	{
		this->Mtbfcount = 0;
		for(int i = 0; i < MMAXGROUP; ++i)
		{
			if( MainMtbfGroup.MtbfGroup[i] != nullptr)
			{
				++ this->Mtbfcount;
			}
		}
	}
public:
	int Mtbfcount;
};

MtbfGroup::MtbfGroup()
{
	int err = 0;
	if((err = pthread_mutex_trylock(&MainMtbfGroup.MtbfGrouplock)) == EBUSY)
	{
		throw "can not create a new MtbfGroup";
		return;
	}
	Groupptr = this;
	this->d = new (MDATA(MtbfGroup))();	
}
MtbfGroup::~MtbfGroup()
{
	delete this->d;
	pthread_mutex_unlock(&MainMtbfGroup.MtbfGrouplock);
}
Mtbf& MtbfGroup::operator[] (int pos)
{
	static Mtbf errMTBF;
	if(MainMtbfGroup.getFreePos(MainMtbfGroup.MtbfGroup[pos]) >= 0)
	{	
		return *(MainMtbfGroup.MtbfGroup[pos]);
	}
	else
	{
		return errMTBF;

	}
}
void MtbfGroup::pushTbf()
{
	++this->d->Mtbfcount;
}
int MtbfGroup::getPos(Mtbf*tbf)
{
	int i = 0;
	for(; i < MMAXGROUP; ++i)
	{
		if(MainMtbfGroup.MtbfGroup[i] == tbf && tbf != nullptr)
		{
			return i;
		}
	}
	return -EINVAL;
}
int MtbfGroup::isFull()
{
	return MMAXGROUP - this->d->Mtbfcount;
}
int MtbfGroup::isMember(Mtbf* tbf)
{
	return this->getPos(tbf);
}
int MtbfGroup::destroyTbf(int pos)
{
	if(this->getPos(MainMtbfGroup.MtbfGroup[pos]))
	{	
		MainMtbfGroup.MtbfGroup[pos]->~Mtbf();
		return pos;
	}
	else
		return -EINVAL;
}
