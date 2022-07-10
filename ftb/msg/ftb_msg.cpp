#include<iostream>
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/mman.h>
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
	void operator=(MDATA(MMsg)&& otherdata)
	{
		memcpy(this->mJob, otherdata.mJob, MMSGMAX);
		this->parent = otherdata.parent;
		otherdata.parent = nullptr;
		for(auto&x : otherdata.mJob)
		{
			x = nullptr;
		}
	}
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
				msgctl(x->msgid, IPC_RMID, NULL);
				munmap(x, sizeof(struct MMsgJob));
				x  = nullptr;
			}
		}
	}
	
	int driverFsm(int md, void *data_address, bool kpstat, long datasize, long mtype )
	{
		struct MMsgJob *job = this->mJob[md];
		int rebytes = 0;
		if(job == nullptr)
		{
			errno = EINVAL;
			perror("driver fsm failed: ");
			throw std::string("job error");
		}
		////mcout("fsm drivering now");
		switch(job->fsm.state)
		{
		case MSTATE_RCV:
			{
				////mcout("MSTATE_RCV go in ");
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
				if(rebytes = (msgrcv(job->msgid, data_address, 
						datasize , 0, 0 )) < 0)
				{
					////mcout("MSTATE_RCV doing");
					if(errno == EAGAIN || errno == EINTR || errno == 0)
					{
						job->fsm.state	= MSTATE_RCV;
						return 0;
					}
					else
					{
						perror("MSTATE_RCV ");
					}
					
					job->fsm.errstr		= strerror(errno);		
					job->fsm.errstate 	= MSTATE_RCV;
					job->fsm.state 		= MSTATE_Ex;
					job->fsm.err		= errno;
					throw job->fsm.errstr;
					return -1;
				}
				////mcout("message get");
				
//				////mcout(rebytes);
//				////mcout(*((int*)(data_address)));	
//				////mcout(*((int*)(data_address) + sizeof(long)));
//				////mcout(*((int*)(data_address) +2 * sizeof(long)));
				
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
					perror("MSTATE_SND");
					if(errno == EINVAL)
					{
						//mcout(job->msgid);
						//mcout(data_address);
						//mcout(datasize);
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


void MMsg::operator=(MMsg&& othermsg)
{
	this->d = othermsg.d;
	othermsg.d = nullptr;	
}

MMsg::MMsg()
{
	MDATA(MMsg) *tmpmsg 	= new MDATA(MMsg)(this);
	this->d 		= (MDATA(MMsg)*)
					   mmap(NULL,
					   sizeof(MDATA(MMsg)),
					   PROT_READ|PROT_WRITE,
					   MAP_SHARED| MAP_ANONYMOUS,
					  -1,0);
	*this->d		= std::move(*tmpmsg);
	delete tmpmsg;
}
MMsg::~MMsg()
{
	if(this->d != nullptr)
	{	
		this->d->~MMsgMdata();
		munmap(this->d, sizeof(MDATA(MMsg)));
		this->d = nullptr;
	}
}

int MMsg:: MMsgRegister(u_int64_t mod, const char* pathname)
{
	if(!((mod & MMSG_RCV) || (mod & MMSG_SEND)))
	{
		errno = EINVAL;
		perror("MMsgRegister():mod error(): ");
		throw std::string("EINVAL");
		return -EINVAL;
	}
	int md = 0;
	//mcout("MMsgRegister work");
	//mcout(&MsgPosLock);
	//mcout(this->d);
	pthread_mutex_lock(&MsgPosLock);
	for(; md < MMSGMAX; ++md)
	{
		//mcout("range_for");
		if(this->d->mJob[md] == nullptr)
		{
			//mcout("md:  ====");	
			//mcout(md);
			break;
		}		
	}
	pthread_mutex_unlock(&MsgPosLock);
	//mcout("pthread unlock");	
	if(md == MMSGMAX)
	{
		errno = ENOMEM;
		perror("MMsgReigster():");
		throw std::string("ENOMEM");
		return -ENOMEM;
	}
	key_t key;
	int msgid = 0;	
	//mcout("ftbk ready");
	//这里一定要记得获取权限，不然无法读写	
	if(mod & MMSG_PUBLIC)
	{
		key 	= ftok(pathname, MPROJ);
		msgid 	= msgget(key, 0666| IPC_CREAT);
	}
	if(mod & MMSG_PRIVATE)
		msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);	
	//mcout("msgid:               ========");
	//mcout(msgid);	
	if(msgid < 0)
	{
		if(errno == EEXIST)
		{
			for(auto& x : this->d->mJob)
			{
				if(x->key == key)
				{
					//perror("msgget():");
					//保证获取的是同一个消息队列
					return x->md;
				}
			}
		}
		else
		{
			perror("MMsgReigster():");
			throw std::string(strerror(errno));
		
			return -errno;
		}
	}
	else
	{
		//mcout("init now");
		this->d->mJob[md] 		= (struct MMsgJob*)
						   mmap(NULL,
						   sizeof(struct MMsgJob),
						   PROT_READ|PROT_WRITE,
						   MAP_SHARED| MAP_ANONYMOUS,
						  -1,0);
		//mcout("mmap finish");
		struct MMsgJob *job	 	= this->d->mJob[md];
		job->state 			= MSTATE_RUNNING;
		job->key 			= key;
		job->msgid			= msgid;
		job->fsm.state			= (mod & MMSG_RCV)?MSTATE_RCV:MSTATE_SND;
		job->fsm.errstate		= MSTATE_AUTO;
		job->fsm.err			= 0;
		job->parent			= this->d;
		job->md				= md;
		//mcout("init finish");
	}
	////mcout("Register success\n");
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
		////mcout(mod);
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
		perror("MMsgCtl():");
		throw std::string("EINVAL");
		return -EINVAL;
	}	
	int err = 0;

//	void *datapack = malloc(sizeof(long) + size);
//	memcpy(datapack, &mtype, sizeof(long));
	
	////mcout("datapack create successly");
	
//	switch(mtype)
//	{
//	case MMSGPATH:
//		memcpy(datapack + sizeof(long), data, size);	
//		break;
//	case MMSGDATA:
//		memcpy(datapack + sizeof(long), data, size);
//		break;
//	case MMSGEND:
//		memcpy(datapack + sizeof(long), data, size);
//		break;
//	default:
//		errno = EINVAL;
//		throw std::string("EIVAL");
//		return EINVAL;
//		break;
//	}
	
	////mcout("ftb_msg: datapack set already");
	if( mod & MMSG_RCV)
	{
		////mcout("MSTATE_RCV");
		this->d->mJob[md]->fsm.state = MSTATE_RCV;	
		err = this->d->driverFsm(md, retdata, (mod & MMSG_KPSTAT)? true: false, size, mtype);
	}
	else if( mod & MMSG_SEND)
	{
		this->d->mJob[md]->fsm.state = MSTATE_SND;
		err = this->d->driverFsm(md, data, (mod & MMSG_KPSTAT)? true: false, size, mtype);
	}
	else if( mod & MMSG_AUTO)
	{
			
		err = this->d->driverFsm(md, data, (mod & MMSG_KPSTAT)? true: false, size, mtype);
	}
	else
	{
		errno = EINVAL;
		perror("mod err:");
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
//	if(retdata != nullptr && (mod & MMSG_RCV))
//	{
//		long smtype = (long)datapack;
//		switch(smtype)
//		{
//		case MMSGPATH:
//			{
//				std::cout<< (char*)(datapack + sizeof(long)) << std::endl;
//				break;
//			}
//		case MMSGDATA:
//			{
//				memcpy(retdata, data, size);
//				break;
//			}
//		case MMSGEND:
//			{
//				std::cout<< (char*)(datapack + sizeof(long)) << std::endl;
//				break;
//			}
//		default:
//			
//			break;
//		}
//	}
//	free(datapack);
	return 0;

}

int MMsg::MMsgDestroy(int md)
{
	struct MMsgJob * job = this->d->mJob[md];
	if(job == nullptr)
	{
		errno = EINVAL;
		perror("MMsgDestory()");
		throw std::string("EINVAL");
		return -EINVAL;
	}
	int err = msgctl(job->msgid, IPC_RMID, NULL);
	if(err < 0 && errno == EPERM)
	{
		return errno;
	}
	
	munmap(job, sizeof(struct MMsgJob));
	job = nullptr;
	this->d->mJob[md] = nullptr;

	return 0;
}

