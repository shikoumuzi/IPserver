#include<iostream>
#include<vector>
#include<signal.h>
#include<errno.h>
#include<unistd.h>
#include<sys/time.h>
#include<time.h>
#include<thread>
#include<atomic>
#include<deque>
#include<algorithm>
#include"Mtoken.h"
#include<pthread.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#define  MMAXTOKEN	 1000
#define  MTA		(size_t)(-1)	

MDATA(Mtoken);
MDATA(MtokenGroup);
struct MsigToken;
static bool Msignalready = false;
static bool Mgrouponly = false;
static void MSignalHandler(int s, siginfo_t* info, void* p);
static void MSignalHandlerForGroup(int s, siginfo_t* info, void *p);

typedef void (*sighandler_t)(int);
typedef void (*sigaction_t)(int, siginfo_t*, void*);

static pthread_mutex_t sigmutex;
static MtokenGroup* mtokengroup = nullptr;


MDATA(Mtoken)
{
public:
	friend struct MsigToken;
	friend void MSignalHandler(int s, siginfo_t* info, void *p);
	friend void MSignalHandlerForGroup(int s, siginfo_t * info, void *p);
	friend class Mtoken;
	friend MDATA(MtokenGroup);
public:
	MtokenMDATA(int sec, int64_t token = MMAXINGROUP, int pos = -1)
	{

		this->sec 		= sec;
		this->token 		= token;
		this->pos 		= pos;
		this->workstat 		= false;
		this->vaildtoken 	= 0;

		this->signtime.it_interval.tv_sec 	= 1;// inerval for periodic timer
		this->signtime.it_interval.tv_usec 	= 0;//
		this->signtime.it_value.tv_sec 		= 1; //starttime;
		this->signtime.it_value.tv_usec 	= 0;
		
		//因为所定义出来的变量初始值是随机的，需要先清空，再赋值	
		sigemptyset(&(this->mask));
		sigaddset(&(this->mask),SIGALRM);
		this->MtokenSiginit();
		
	}
	void MtokenSiginit(int sa_flags = 0)
	{
		this->sigmaction.sa_sigaction   = MSignalHandler;
		this->sigmaction.sa_mask	= this->mask;
		this->sigmaction.sa_flags	= 0;
		this->sigmaction.sa_restorer	= nullptr;
	}
public:
	//all of them is safe in thread
	std::atomic<int> sec; //时间
	sig_atomic_t token; //令牌
	sig_atomic_t vaildtoken;
	sigset_t mask;
	//sigset_t是typedef抽象出来的可以容纳所有信号值的整形数

	int pos; //下标
	bool workstat;
	bool ishasp;

	struct itimerval signtime;// an new time;
	struct sigaction sigmaction;
};

static struct MsigToken
{
private:
	std::deque<Mtoken*> tokenqueue;
public:
	void doToken()
	{

		for(std::deque<Mtoken*>::iterator it = this-> tokenqueue.begin(); 
				it != this->tokenqueue.end(); ++it)
		{
			--((*it)->d->token);
			++((*it)->d->vaildtoken);
//			std::cout<<"MsignalHandler: "<<(*it)->d->vaildtoken<<std::endl;
		}		
	}
	void pushToken(Mtoken* token)
	{	
		if(this->tokenqueue.size() > MMAXINGROUP)
		{
			errno = ENOSPC;
			exit(1);
		}
		this->tokenqueue.push_back(token);
	}
}MsigTokenQueue;


static void MSignalHandler(int s, siginfo_t* info, void* p)
{
//	std::cout << "MSignalHandler: "<<std::endl;
	MsigTokenQueue.doToken();
}

MDATA(MtokenGroup)
{
public:
	friend void MSignalTokenForGroup(int s, siginfo_t* info, void *p);		
public:
	MtokenGroupMDATA()
	{
		this->data.resize(MMAXINGROUP,nullptr);
		this->max = MMAXINGROUP;
		this->count = 0;
		sigmutex = PTHREAD_MUTEX_INITIALIZER;
	}
	~MtokenGroupMDATA()
	{
		for(int i = 0; i < this->data.size(); ++i)
		{
			if(this->data[i] != nullptr)
			{
				if( this->data[i]->d->pos != -1)
				{	
					delete this->data[i];
				}
				else
				{
					this->data[i]->~Mtoken();
				}
				this->data[i] = nullptr;
			}
		}
	}
public:
	std::vector<Mtoken*> data;
	int max;
	int count;
};

static void MSignalHandlerForGroup(int s, siginfo_t* info, void *p)
{
	int i = 0;
	++i;	
}


Mtoken::Mtoken():d(nullptr){};
Mtoken::Mtoken(int sec, int64_t token, int pos)
	:d(new MDATA(Mtoken)(sec,token,pos)){}
Mtoken::~Mtoken()
{
	delete this->d;
}

bool Mtoken::TokenStart(int signum)
	//function setitimer need an option to choose mod to start itself
{
	if(Msignalready == false)
	{	
		std::cout<<"signal is ready "<<std::endl;
		int err = 0;
		//相当于signal的替换函数	
		if((err = sigaction(SIGALRM,&(this->d->sigmaction),NULL)) <  0)
		{
			perror("TokenStart failed (sigaction): ");
			exit(1);
		}

		//alarm(5);	
		//相当于alarm的替换函数
		if(err=	setitimer(ITIMER_REAL, &(this->d->signtime), NULL) < 0)
		{
			perror("TokenStart faild (seritimer)");
			exit(1);
		}
		Msignalready = true;
	}
	this->d->workstat = true;
	MsigTokenQueue.pushToken(this);
	return true;
}

sigset_t Mtoken::getMask()
{
	return this->d->mask;
}

void Mtoken::changeMask(const sigset_t* mask)
{
	this->d->mask = *mask;
	sigaddset(&(this->d->mask),SIGALRM);
}

int Mtoken::getVaildToken()
{
	while(this->d->vaildtoken != this->d->sec && this->d->workstat)
	{
	//	std::cout<<"getValdToken: "<<this->d->vaildtoken<<" "<< std::endl;
		pause();
	}

	this->d->token 		+= this->d->vaildtoken;
	int rt 			= this->d->vaildtoken;
	this->d->vaildtoken	= 0;
	return rt;
}	

bool Mtoken::TokenEnd()
{
	this->d->workstat = false;
	return true;	
}

bool Mtoken::TokenDestory()
{
	this->d->workstat = false;
}
/*---------------------------------------------------------*/
MtokenGroup::MtokenGroup()
	:d(new MDATA(MtokenGroup)()){}
MtokenGroup::MtokenGroup(MtokenGroup&& othergroup)
	:d(othergroup.d)
{
	othergroup.d = nullptr;
}
MtokenGroup::~MtokenGroup()
{
	delete this->d;
}

void MtokenGroup::createMtokenGroup()
{
	this->d = new MDATA(MtokenGroup)(); 
}

int MtokenGroup::JudGroupPos(int pos)
{
	if(pos > this->d->max)
	{
		errno = -EINVAL;
		throw "out of range";
		
		return -EINVAL;
	}
	if(this->d->data[pos] == nullptr)
	{

		errno = EADDRNOTAVAIL;
		throw "this address is nullptr";
		return -EADDRNOTAVAIL;
	}	
	return pos;
}

int MtokenGroup::autoToken(int sec, int64_t token)
{	
	Mtoken *temp = (new Mtoken(sec, token));
	return this->pushToken(&temp, true);
}

int MtokenGroup::pushToken(Mtoken** otherdata, bool ishasp)
{	
	if(otherdata == nullptr)
	{
		//无效参数
		errno = EINVAL;
		return -EINVAL;
	}
	if(this->d->count < this->d->max)
	{
		
		int i = 0;
		for(i = 0; i < this->d->count; ++i)
		{
			if(this->d->data[i] == nullptr)
			{
				break;
			}		
		}
		++this->d->count;

		this->d->data[i] = new Mtoken((*otherdata)->d->sec, (*otherdata)->d->token, i);
		memcpy(this->d->data[i], (*otherdata), sizeof(Mtoken));
		this->d->data[i]->d->sigmaction.sa_sigaction = MSignalHandlerForGroup;
		
		if(ishasp == true)
			delete (*otherdata);
		
		(*otherdata) = this->d->data[i];

		return i;
	}
	else
	{
		errno = EMSGSIZE;
		return -EMSGSIZE;	
	}
}

int MtokenGroup::delToken(int pos)
{
	int rt = 0;
	if((rt = this->JudGroupPos(pos)) != pos)
	{
		return rt;
	}

	
	if(this->d->data[pos]->d->pos != -1)
	{
		delete this->d->data[pos];
	}
	else
	{
		this->d->data[pos]->~Mtoken();
	}
	this->d->data[pos] = nullptr;
	return pos;
}

Mtoken& MtokenGroup::operator[](int pos)
{
	if(pos >= this->d->max)
	{
		errno = EINVAL;
		throw "out of range";
	       	exit(1);
	}
	if((this->d->data[pos]) == nullptr)
	{
		static Mtoken errMtoken;
		return errMtoken;
	}
	else
	{
		return *(this->d->data[pos]);
	}

}

int MtokenGroup::TokenStart(int pos)
{
	if(pos == MTA)
	{

	}
	else
	{
		int rt = 0;
		if((rt = this->JudGroupPos(pos)) != pos)
		{
			return rt;
		}
		else
		{
			
		}
	}
}
int MtokenGroup::TokenEnd(int pos)
{
	if(pos == MTA)
	{

	}
	else
	{
		int rt = 0;
		if((rt = this->JudGroupPos(pos)) != pos)
		{
			return rt;
		}
		else
		{
			
		}
	}
}


