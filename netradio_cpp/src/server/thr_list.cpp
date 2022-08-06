#include<unistd.h>
#include<pthread.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<syslog.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<net/if.h>

#include<string.h>

#include<iostream>
#include<string>

#include<ipv4serverproto.h>
#include"server_conf.h"
#include"sockres.h"
#include"thr_list.h"
namespace MUZI{

MPRIVATE(ThrMediaList):public MIpv4ServerProto, public SockRes
{
public:
	ThrMediaListMPrivate(SockRes * sockres):SockRes()
	{
		if(sockres != nullptr)
		{
			this->setSockData(*sockres);
			return;
		}	
		this->SockInit();
		sockres = new SockRes(this->getSockRes());	
		
	}
	~ThrMediaListMPrivate()
	{
		pthread_cancel(this->tid_list);
		pthread_join(this->tid_list, NULL);
	}	
public:
	struct ThrListRes
	{

		msg_list_t *entlistptr;
	};	
	static void cleanThrList(void* arg)
	{
		ThrListRes* thrresptr = (ThrListRes*)arg;
		free(thrresptr->entlistptr);
	};

	static void*thr_list(void *p)
	{
		MPRIVATE(ThrMediaList)* thrdataptr = (MPRIVATE(ThrMediaList)*)p;

		struct ThrListRes entlist_st;
		entlist_st.entlistptr 	= nullptr;
		pthread_cleanup_push(cleanThrList, &entlist_st);
		int totalsize 		= sizeof(chnid_t);
		msg_list_t * &entlistp 	= entlist_st.entlistptr;
		msg_listentry_t *entryp = nullptr;

		for(int i = 0 ; i < thrdataptr->nr_list_ent; ++i)
		{
			totalsize += sizeof(msg_listentry_t) + thrdataptr->list_ent[i].desc->size();
		}

		entlistp 		= (msg_list_t*)malloc(totalsize);
		if(entlistp  == NULL)
		{
			syslog(LOG_ERR, "malloc(): %s", strerror(errno));
		} 

		entlistp->chnid 	= LISTCHNID;
		entryp 			= entlistp->entry;

		for(int i = 0; i < thrdataptr->nr_list_ent; ++i)
		{
			int partsize 	= sizeof(msg_listentry_t) + thrdataptr->list_ent[i].desc->size();

			entryp->chnid 	= thrdataptr->list_ent[i].chnid;
			entryp->len 	= htons(partsize);
			strcpy((char*)entryp->desc,thrdataptr->list_ent[i].desc->c_str());
			entryp 		= (msg_listentry_t*)(((char*)entryp) + partsize);
		}

		struct timeval timeout 	= {\
					.tv_sec = 1,
					.tv_usec = 0,
					};

		msg_list_t* tmptr 	= entlistp;
		//for(int i = 0; i < nr_list_ent; ++i)
		//{
		//	int size = sizeof(msg_listentry_t) + strlen(list_ent[i].desc);
		//	syslog(LOG_DEBUG, "%d : %s", tmptr->entry->desc);
		//}
		int ret = 0;
		while(1)
		{
			ret = sendto(thrdataptr->getSockfd(), entlistp, totalsize, 0 , 
					(struct sockaddr*)(thrdataptr->getSockaddr()), sizeof(struct sockaddr_in));
			if(ret < 0)
			{
				if(errno == EINTR || errno == EAGAIN)
				{
					continue;
				}
				else
				{
					syslog(LOG_WARNING, "thr_list: sendto(): %s", strerror(errno));
				}
			}	
			else
			{
				syslog(LOG_DEBUG, "thr_list: sendto():ret:%d :%s ", ret, strerror(0));
			}
			select(0, NULL, NULL, NULL, &timeout);
			sleep(1);
			
			pthread_testcancel();
			sched_yield();
		}
		pthread_cleanup_pop(1);
	}
public:

	pthread_t tid_list;
	int nr_list_ent;
	mlib_listentry_t *list_ent;
};


ThrMediaList::ThrMediaList(SockRes *sockres)
{
	this->d = new MPRIVATE(ThrMediaList)(sockres);
}
ThrMediaList::~ThrMediaList()
{
	delete this->d;
}
int ThrMediaList::create(mlib_listentry_t *mliblist , int size)
{
	int err = 0;
	this->d->list_ent = mliblist;
	this->d->nr_list_ent = size;
	err = pthread_create(&this->d->tid_list, NULL, this->d->thr_list, this->d);
	if(err)
	{
		syslog(LOG_ERR, "pthread_create(): %s", strerror(err));
		return -1;
	}
	return 0;
}
};
