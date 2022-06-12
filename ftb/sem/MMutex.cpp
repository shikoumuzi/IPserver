#include<stdlib.h>
#include<pthread.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<string>
#include<string.h>
#include<errno.h>

#include"MMutex.h"
struct MutexJobs
{
	int mtid;
	int islock:1;
};

MDATA(MMutex)
{
public:
	MMutexMdata(MMutex* parent)
	{
		this->parent = parent;
		this->key = ftok(MPATHNAME, MPROJ);
		this->semid = semget(this->key, MMUTEXSMAX, IPC_CREAT | 0666);
		for(int i = 0; i <  MMUTEXSMAX; ++i)
		{
			semctl(this->semid, i, SETVAL, 1);
		}
		for(int i = 0; i < MMUTEXSMAX; ++i)
		{
			this->MutJobs[i] = nullptr; 
		}
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_FAST_NP);
		pthread_mutex_init(&this->lockpos, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	~MMutexMdata()
	{
		pthread_mutex_unlock(&this->lockpos);
		pthread_mutex_destroy(&this->lockpos);	
		for(int i = 0; i < MMUTEXSMAX; ++i)
		{
			semctl(this->semid, i, IPC_RMID);
			for(auto& x : this->MutJobs)
			{
				if(x != nullptr)
				{
					delete x;
					x = nullptr;
				}
			}
		}
	}
public:
	pthread_mutex_t lockpos;
	struct MutexJobs * MutJobs[MMUTEXSMAX];
	MMutex *parent;
	key_t key;
	int semid;
};

class autoLock
{
public:
	autoLock(int mtid, MMutex * parent)
	{
		this->mtid 	= mtid;
		this->parent 	= parent;
		this->parent->Lock(mtid);
	}
	~autoLock()
	{
		this->parent->unLock(this->mtid);
	}
public:
	MMutex* parent;
	int mtid;	
};

MMutex::MMutex():d(new MDATA(MMutex)(this))
{}
MMutex::~MMutex()
{
	delete this->d;
}
int MMutex::getLock()
{
	int i = 0;
	for(; i < MMUTEXSMAX; ++i)
	{
		if(this->d->MutJobs[i] == nullptr)
		{
			this->d->MutJobs[i] 		= new struct MutexJobs;
			this->d->MutJobs[i]->mtid 	= i;
			this->d->MutJobs[i]->islock 	= 0;
			break;
		}
	}
	return i;
}
int MMutex::Lock(int mtid)
{
	if(mtid < 0 || mtid > MMUTEXSMAX  || this->d->MutJobs[mtid] == nullptr)
	{
		errno = EINVAL;
		perror("MMurex::Lock: ");
		throw std::string("EINVAL");
		return -EINVAL;
	}
	struct sembuf semopt;
	semopt.sem_num	= mtid;
	semopt.sem_op	= -1;
	semopt.sem_flg	= 0;
	semop((int)this->d->semid, &semopt, 1);	
	
	this->d->MutJobs[mtid]->islock = 1;
	return 0;
}
int MMutex::unLock(int mtid)
{
	if(mtid < 0 || mtid > MMUTEXSMAX  || this->d->MutJobs[mtid] == nullptr)
	{
		errno = EINVAL;
		perror("MMurex::Lock: ");
		throw std::string("EINVAL");
		return -EINVAL;
	}
	struct sembuf semopt;
	semopt.sem_num	= mtid;
	semopt.sem_op	= 1;
	semopt.sem_flg	= 0;
	semop((int)this->d->semid, &semopt, 1);	

	this->d->MutJobs[mtid]->islock = 0;
	return 0;
}
class autoLock MMutex::autoLock()
{
	class autoLock ret(this->getLock(), this);
	return ret;
}
int MMutex::Lockable(int mtid)
{
	if(mtid < 0 || mtid > MMUTEXSMAX  || this->d->MutJobs[mtid] == nullptr)
	{
		errno = EINVAL;
		perror("MMurex::Lock: ");
		throw std::string("EINVAL");
		return -EINVAL;
	}
	return (this->d->MutJobs[mtid]->islock);
}
bool MMutex::LockStat(int mtid)
{
	return (this->Lockable(mtid) == 1)?true:false;
}


