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
#include"thr_list.h"
namespace MUZI{

MPRIVATE(ThrMediaList):public MIpv4ServerProto
{
public:
	ThrMediaListMPrivate(int sd, struct sockaddr_in& sndaddr)
	{
		this->socksd 		= sd;
		this->nr_list_ent 	= 0;
		this->list_ent 		= nullptr;
		this->sndaddr		= sndaddr;
	}
	ThrMediaListMPrivate(int* sd, struct ip_mreqn* mreqnrt, struct sockaddr_in * sndaddrrt)
	{
		if(sd == nullptr || mreqnrt == nullptr || sndaddrrt == nullptr)
		{
			syslog(LOG_WARNING, "ThrMediaList::ThrMediaList(): %s", strerror(EINVAL));
			errno = EINVAL;
			throw std::string("ERR_INVAL");
			return;
		}
		int socksd = 0; 
		if(socksd = socket(AF_INET, SOCK_DGRAM, 0) < 0)
		{
			syslog(LOG_ERR, "ThrMediaList::ThrMediaList():socket(): %s", strerror(errno));
			throw std::string("ERR_SOCK");	
			return;
		}

		inet_pton(AF_INET, serverconf.mgroup, &mreqnrt->imr_multiaddr);
		inet_pton(AF_INET, "0.0.0.0", &mreqnrt->imr_address);
		mreqnrt->imr_ifindex	= if_nametoindex(serverconf.ifname);	
		
		if(setsockopt(socksd, IPPROTO_TP, IP_MULTICAST_IF, mreqnrt, sizeof(*mreqnrt)) < 0)
		{
			syslog(LOG_ERR, "ThrMediaList::ThrMediaList():setssockopt(): %s", strerror(errno));
			throw std::string("ERR_SOCK");	
			return;
		}
		this->mreqn 		= *mreqnrt;
		sndaddrrt->sin_family 	= AF_INET;
		sndaddrrt->sin_port 	= htons(atoi(serverconf.rcvport));
		inet_pton(AF_INET, "0.0.0.0", &sndaddrrt->sin_addr);
		this->sndaddr		= *sndaddrrt;
		
		
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
			totalsize += sizeof(msg_listentry_t) + thrdataptr->list_ent[i].desc.size();
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
			int partsize 	= sizeof(msg_listentry_t) + thrdataptr->list_ent[i].desc.size();

			entryp->chnid 	= thrdataptr->list_ent[i].chnid;
			entryp->len 	= htons(partsize);
			strcpy((char*)entryp->desc,thrdataptr->list_ent[i].desc.c_str());
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
			ret = sendto(serversd, entlistp, totalsize, 0 , 
					(struct sockaddr*)&(thrdataptr->sndaddr), sizeof(struct sockaddr_in));
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
			sched_yield();
		}
		pthread_cleanup_pop(1);
	}
public:

	pthread_t tid_list;
	int nr_list_ent;
	mlib_listentry_t *list_ent;
	struct sockaddr_in sndaddr;
	struct ip_mreqn mreqn;
	int socksd;
};

ThrMediaList::ThrMediaList(int sd, struct sockaddr_in& sndaddr)
{
	this->d = new MPRIVATE(ThrMediaList)(sd, sndaddr);
}

ThrMediaList::ThrMediaList(int* sd, struct ip_mreqn* mreqnrt, struct sockaddr_in * sndaddrrt)
{
	if(sd == nullptr || mreqnrt == nullptr || sndaddrrt == nullptr)
	{
		syslog(LOG_WARNING, "ThrMediaList::ThrMediaList(): %s", strerror(EINVAL));
		errno = EINVAL;
		throw std::string("ERR_INVAL");
		return;
	}
	this->d = new MPRIVATE(ThrMediaList)(sd, mreqnrt, sndaddrrt);
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
