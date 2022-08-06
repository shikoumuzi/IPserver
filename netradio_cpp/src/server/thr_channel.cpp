#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<syslog.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<net/if.h>

#include<ipv4serverproto.h>
#include"thr_channel.h"
#include"server_conf.h"
#include"sockres.h"

#include<iostream>
#include<vector>
namespace MUZI{

static int tid_nextpos = 0;
typedef struct ThrChannelEntry
{
	chnid_t chnid;
	pthread_t tid;
} thr_channel_ent_t ;

struct ThrMediaChannelRes
{
	mlib_listentry_t *entry;	
	MPRIVATE(ThrMediaChannel) *d;
};

MPRIVATE(ThrMediaChannel):public MIpv4ServerProto, public SockRes 
{
private:
	ThrMediaChannelMPrivate():SockRes()
	{
		this->thr_channel.resize(CHNNR, nullptr);
	}
public:
	ThrMediaChannelMPrivate(MediaLib& media, SockRes* sockres):ThrMediaChannelMPrivate()

	{

		this->media = &media;	
		if(sockres != nullptr)
		{
			this->setSockData(*sockres);	
			return;
		}
		this->SockInit();
		sockres = new SockRes(this->getSockRes());		
	}
	~ThrMediaChannelMPrivate()
	{
		for(auto& x : this->thr_channel)
		{
			if( x != nullptr)
			{

				pthread_cancel(x->tid);
				pthread_join(x->tid, NULL);
				delete x;
				x = nullptr;
			}
		}
		this->media = nullptr;
		//this->~SockRes();
		//这里不需要自行调用父类的析沟函数
		//delete会自行调用
	}
public:
	int getChannelPos(thr_channel_ent_t* ent)
	{
		int i = 0;
		for(auto& x : this->thr_channel)
		{
			if(x == nullptr)
			{
				x = ent;
				return i;
			}
			++i;
		}
		return -1;
	}
public:
	static void cleanThrChannel(void *p)
	{
		//free(((struct ThrMediaChannelRes*)p) -> entry);
		delete (struct ThrMediaChannelRes*)p;	
	}
	static void* thr_channel_snder(void *ptr)
	{
		msg_channel_t *sbufp 			= (msg_channel_t*)malloc(MSG_CHANNEL_MAX);
		pthread_cleanup_push(cleanThrChannel, sbufp);
		int len = 0;
		if(sbufp == NULL)
		{
			syslog(LOG_ERR,"malloc():%s\n", strerror(errno));
			exit(1);
		}
		sbufp->chnid 				= ((struct ThrMediaChannelRes*)ptr)->entry->chnid;
		struct ThrMediaChannelRes* res 		= (struct ThrMediaChannelRes*)ptr;
		MPRIVATE(ThrMediaChannel)* thrdataptr 	= res->d;	
		int ret 				= 0;
		while(1)
		{
			len = thrdataptr->media->readchnl(sbufp->chnid, sbufp->data, MSG_DATA_MAX);

			if((ret = sendto(thrdataptr->getSockfd(),sbufp, len + sizeof(chnid_t), 0, 
						(struct sockaddr*)thrdataptr->getSockaddr(),sizeof(struct sockaddr_in))) < 0 )
			{
				syslog(LOG_ERR, "thr_channel(%d):sendto():%s\n", 
						res->entry->chnid, strerror(errno));
			}
			else
			{
				syslog(LOG_DEBUG, "thread id %d, thr_channel(%d):sendto():ret:%d\n",
						pthread_self(), res->entry->chnid,ret);
			}
			sched_yield();
			//主动出让调度期
			//这里如果设置了pthread_testcancel的话，
			//当调用pthread_cancel时会直接取消线程
			//导致pthread_join段错误
			++sbufp->id;
		}
		pthread_cleanup_pop(1);
		pthread_exit(NULL);
	}

public:
	std::vector<thr_channel_ent_t*> thr_channel;
	MediaLib* media;
};

ThrMediaChannel::ThrMediaChannel(MediaLib& media, SockRes* sockres)
{
	this->d = new ThrMediaChannelMPrivate(media, sockres);
}
ThrMediaChannel::~ThrMediaChannel()
{
	delete this->d;
}

int ThrMediaChannel::create(mlib_listentry_t *ptr )
{
	thr_channel_ent_t* me 		= new thr_channel_ent_t;
	struct ThrMediaChannelRes *res 	= new struct ThrMediaChannelRes;
	res->entry 			= ptr;
	res->d				= this->d;
	int err 			= 0;
	int tid_nextpos 		= this->d->getChannelPos(me);

	if((err = pthread_create(&me->tid, NULL, 
					this->d->thr_channel_snder, res)))
	{
		syslog(LOG_WARNING,"pthread_create():%s", strerror(err));
		return -err;
	}
	me->chnid = ptr->chnid;
	return 0;
}

int ThrMediaChannel::destroyonce(mlib_listentry_t * ptr)
{
	
	for(int i = 0; i < CHNNR; ++i)
	{
		if(this->d->thr_channel[i] != nullptr)
		{	
			if(this->d->thr_channel[i]->chnid == ptr->chnid)
			{
				if(pthread_cancel(this->d->thr_channel[i]->tid) < 0)
				{
					syslog(LOG_ERR,"pthread_cancel()): the thread of channel %d", 
							ptr->chnid);
					return -ESRCH;
				}	
				pthread_join(this->d->thr_channel[i]->tid, NULL);
				delete this->d->thr_channel[i];
				return 0;
			}
		}
	}
	return -1;
}

int ThrMediaChannel::destroyall()
{
	for(auto& x : this->d->thr_channel)
	{
		if(x != nullptr)
		{
			pthread_cancel(x->tid);
			pthread_join(x->tid, NULL);
			delete x;
			x = nullptr;
		}
	}
	return 0;
}
};
