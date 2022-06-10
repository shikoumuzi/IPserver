#include<iostream>
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<errno.h>
#include<pthread.h>
#include"ftb_msg.h"
#include<string.h>

static pthread_mutex_t MsgPosLock = PTHREAD_MUTEX_INITIALIZER;

enum
{//fsm's state
	MSTATE_RCV = 0x00000001UL,
	MSTATE_SND,
	MSTATE_AUTO,
	MSTATE_Ex,
	MSTATE_T
};

enum
{//job's state
	MSTATE_RUNNING = 0X00000010UL,
	MSTATE_CANEL,
	MSTATE_WAIT,
};
struct MMsgFsm
{
	u_int64_t state;
	u_int64_t errstate;
	std::string errstr;
	int err;
};

struct MMsgJob
{
	key_t key;
	int msgid;

	u_int64_t state;
	struct MMsgFsm fsm;

	MDATA(MMsg) *parent;
	int md;
};
MDATA(MMsg)
{
public:
	MMsgMdata(MMsg* parent)
	{
		this->parent = parent;
		for(auto& x : this->mJob)
		{
			x = nullptr;
		}
	}
	~MMsgMdata()
	{
		for(auto& x : this->mJob)
		{
			if( x != nullptr)
			{
				msgctl(x->md, IPC_RMID, NULL);
				delete x;
				x  = nullptr;
			}
		}
	}
	
	int driverFsm(int md, void *data_address, bool kpstat, long datasize, long mtype )
	{
		struct MMsgJob *job = this->mJob[md];
		int err = 0;
		if(job == nullptr)
		{
			errno = EINVAL;
			perror("driver fsm failed: ");
			throw std::string("job error");
		}
		mcout("fsm drivering now");
		switch(job->fsm.state)
		{
		case MSTATE_RCV:
			{
				mcout("MSTATE_RCV go in ");
				if(data_address == nullptr)
				{
					errno 			= EINVAL;
					job->fsm.state 		= MSTATE_Ex;
					job->fsm.errstate 	= MSTATE_RCV;
					job->fsm.errstr 	= strerror(errno);
					job->fsm.err		= errno;
					
					throw job->fsm.errstr;
					perror("MSTATE_RCV");
					return -1;
				}
				if(msgrcv(job->msgid, data_address, 
						datasize , mtype, 0 ) < 0)
				{
					mcout("MSTATE_RCV doing");
					if(errno == EAGAIN || errno == EINTR || errno == 0)
					{
						job->fsm.state	= MSTATE_RCV;
						return 0;
					}
					else
					{
						perror("driver faild: ");
					}
					
					job->fsm.errstr		= strerror(errno);		
					job->fsm.errstate 	= MSTATE_RCV;
					job->fsm.state 		= MSTATE_Ex;
					job->fsm.err		= errno;
					throw job->fsm.errstr;
					return -1;
				}
				mcout("message get");

				mcout(*((int*)(data_address)));	
				mcout(*((int*)(data_address) + sizeof(int)));

				job->fsm.state = (!kpstat)? MSTATE_SND: MSTATE_RCV;
				return 0;
				break;
			}
		case MSTATE_SND:
			{
				if(data_address == nullptr)
				{
					errno 			= EINVAL;
					job->fsm.state 		= MSTATE_Ex;
					job->fsm.errstate 	= MSTATE_SND;
					job->fsm.errstr 	= strerror(errno);
					job->fsm.err		= errno;
					throw job->fsm.errstr;
					return -1;
					break;
				}
				if(msgsnd(job->msgid, data_address, 
							datasize , 0) < 0)
				{
					if(errno == EAGAIN || errno ==EINTR)
					{
						job->fsm.state 	= MSTATE_SND;
						return 0;
					}
					job->fsm.errstr		= strerror(errno);		
					job->fsm.errstate 	= MSTATE_SND;
					job->fsm.state 		= MSTATE_Ex;
					job->fsm.err		= errno;
					throw job->fsm.errstr;
					return -1;
				
				}
				
				job->fsm.state = (!kpstat)?MSTATE_RCV:MSTATE_SND;
				return 0;
				break;
			}
		case MSTATE_Ex:
			{
			
				break;
			}
		case MSTATE_T:
			{
				/*delete this job*/
				this->parent->MMsgDestroy(job->md);
				return 1;
				break;
			}
		default:
			perror("no mod can match it ");
			abort();
			break;
		}
		return 0;
	}
public:
	struct MMsgJob* mJob[MMSGMAX];
       	MMsg* parent;
};

MMsg::MMsg():d(new MDATA(MMsg)(this)){}
MMsg::~MMsg()
{
	delete this->d;
}

int MMsg:: MMsgRegister(u_int64_t mod, const char* pathname)
{
	if(!((mod & MSTATE_RCV) || (mod & MSTATE_SND)))
	{
		errno = EINVAL;
		throw std::string("EINVAL");
		return -EINVAL;
	}
	int md = 0;

	pthread_mutex_lock(&MsgPosLock);
	for(; md < MMSGMAX; ++md)
	{
		if(this->d->mJob[md] == nullptr)
			break;		
	}
	pthread_mutex_unlock(&MsgPosLock);
	
	if(md == MMSGMAX)
	{
		errno = ENOMEM;
		throw std::string("ENOMEM");
		return -ENOMEM;
	}
	key_t key;
	int msgid = 0;	
	
	if(mod & MMSG_PUBLIC)
	{
		key 	= ftok(pathname, MPROJ);
		msgid 	= msgget(key, IPC_CREAT);
	}
	if(mod & MMSG_PRIVATE)
		msgid = msgget(IPC_PRIVATE, IPC_CREAT);	
	
	if(msgid < 0)
	{
		if(errno == EEXIST)
		{
			for(auto& x : this->d->mJob)
			{
				if(x->key == key)
				{
					return x->md;
				}
			}
		}
		else
		{
			throw std::string(strerror(errno));
			return -errno;
		}
	}
	else
	{
		this->d->mJob[md] 		= new struct MMsgJob;
		struct MMsgJob *job	 	= this->d->mJob[md];
		job->state 			= MSTATE_RUNNING;
		job->key 			= key;
		job->msgid			= msgid;
		job->fsm.state			= (mod & MMSG_RCV)?MSTATE_RCV:MSTATE_SND;
		job->fsm.errstate		= MSTATE_AUTO;
		job->fsm.err			= 0;
		job->parent			= this->d;
		job->md				= md;
	}
	mcout("Register success\n");
	return md;
}

int MMsg::MMsgCtl(int md, u_int64_t mod, void* data, void*retdata, long size, long mtype)
{
	//void driverFsm(int md, void *data_address, bool kpstat = false, long mtype = 0)
	if(!((mod & MMSG_KPSTAT) || (mod & MMSG_CHSTAT)) 
		|| (data == nullptr )
		|| (retdata == nullptr && ((mod & MMSG_RCV) || (mod & MMSG_AUTO)))
		|| this->d->mJob[md] == nullptr
		|| mtype < 0
		|| mod & 0xFFFFFF00UL)
	{
		mcout(mod);
		fprintf(stderr, "\
				!((mod & MMSG_KPSTAT) || (mod & MMSG_CHSTAT)): %d\n \
				data == nullptr: %d\n \
				(retdata == nullptr && ((mod & MMSG_RCV) || (mod & MMSG_AUTO))): %d\n \
				this->d->mJob[md] == nullptr: %d\n \
				mtype < 0: %d\n \
				mod & 0xFFFFFF00UL: %d\n ", 
				!((mod & MMSG_KPSTAT) || (mod & MMSG_CHSTAT)),
				data == nullptr,
				 (retdata == nullptr && ((mod & MMSG_RCV) || (mod & MMSG_AUTO))),
				 this->d->mJob[md] == nullptr,
				 mtype < 0,
				 mod & 0xFFFFFF00UL); 
		perror("ftb_msg: MMsgCtl() ");
		errno = EINVAL;
		throw std::string("EINVAL");
		return -EINVAL;
	}	
	int err = 0;

	void *datapack = malloc(sizeof(long) + size);
	memcpy(datapack, &mtype, sizeof(long));
	
	mcout("datapack create successly");
	
	switch(mtype)
	{
	case MMSGPATH:
		memcpy(datapack + sizeof(long), data, size);	
		break;
	case MMSGDATA:
		memcpy(datapack + sizeof(long), data, size);
		break;
	case MMSGEND:
		memcpy(datapack + sizeof(long), data, size);
		break;
	default:
		errno = EINVAL;
		throw std::string("EIVAL");
		return EINVAL;
		break;
	}
	
	mcout("ftb_msg: datapack set already");
	if( mod & MMSG_RCV)
	{
		mcout("MSTATE_RCV");
		this->d->mJob[md]->fsm.state = MSTATE_RCV;	
		err = this->d->driverFsm(md, datapack, (mod & MMSG_KPSTAT)? true: false, size, mtype);
	}
	else if( mod & MMSG_SEND)
	{
		this->d->mJob[md]->fsm.state = MSTATE_SND;
		err = this->d->driverFsm(md, datapack, (mod & MMSG_KPSTAT)? true: false, size, mtype);
	}
	else if( mod & MMSG_AUTO)
	{
			
		err = this->d->driverFsm(md, datapack, (mod & MMSG_KPSTAT)? true: false, size, mtype);
	}
	else
	{
		errno = EINVAL;
		throw std::string("EINVAL");
		return -EINVAL;
	}
	if(err != 0)
	{
		struct MMsgJob *job = this->d->mJob[md];
		switch(err)
		{
		case 1: 
			{
				errno = ENOSPC;
				return -ENOSPC;
				break;
			}
		case -1:
			{
				switch(job->fsm.err)
				{
				case EFAULT:
					perror("MMsgCtrl():");
					break;
				case EIDRM:
					perror("MMsgCtrl():");
					break;
				default:
					errno = job->fsm.err;
					perror("can't driver fsm: ");
					throw std::string(strerror(errno));
					return -errno;
				}	
				break;
			}

		}
		return -1;
	}
	if(retdata != nullptr)
	{
		long smtype = (long)datapack;
		switch(smtype)
		{
		case MMSGPATH:
			{
				std::cout<< (char*)(datapack + sizeof(long)) << std::endl;
				break;
			}
		case MMSGDATA:
			{
				memcpy(retdata, datapack + sizeof(long), size);
				break;
			}
		case MMSGEND:
			{
				std::cout<< (char*)(datapack + sizeof(long)) << std::endl;
				break;
			}
		default:
			
			break;
		}
	}
	free(datapack);
	return 0;

}

int MMsg::MMsgDestroy(int md)
{
	const struct MMsgJob * job = this->d->mJob[md];
	if(job == nullptr)
	{
		errno = EINVAL;
		throw std::string("EINVAL");
		return -EINVAL;
	}
	int err = msgctl(job->msgid, IPC_RMID, NULL);
	if(err < 0 && errno == EPERM)
	{
		return errno;
	}
	
	delete job;
	job = nullptr;
	this->d->mJob[md] = nullptr;

	return 0;
}

