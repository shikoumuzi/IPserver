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
#include<ipv4serverproto.h>
#include"thr_channel.h"
#include"server_conf.h"
static int tid_nextpos = 0;
struct thr_channel_ent_st
{
	chnid_t chnid;
	pthread_t tid;
};

struct thr_channel_ent_st thr_channel[CHNNR];

static void* thr_channel_snder(void *ptr)
{
	msg_channel_t *sbufp = (msg_channel_t*)malloc(MSG_CHANNEL_MAX);
	int len = 0;
	
	if(sbufp == NULL)
	{
		syslog(LOG_ERR,"malloc():%s\n", strerror(errno));
		exit(1);
	}
	
	sbufp->chnid = ((msg_channel_t*)ptr)->chnid;

	while(1)
	{
		len = mlib_readchnl(((msg_channel_t*)ptr)->chnid, sbufp->data, MSG_DATA_MAX);
		

		if(sendto(serversd,sbufp, len + sizeof(chnid_t), 0, (struct sockaddr*)&sndaddr,
				sizeof(sndaddr)) < 0 )
		{
			syslog(LOG_ERR, "thr_channel(%d):sendto():%s", ((msg_channel_t*)ptr)->chnid, strerror(errno));
		}
		sched_yield();
		//主动出让调度期
	}


	pthread_exit(NULL);



}

int thr_channel_create(mlib_listentry_t *ptr )
{
	int err = 0;
	if((err = pthread_create(&thr_channel[tid_nextpos].tid, NULL, thr_channel_snder, NULL)))
	{
		syslog(LOG_WARNING,"pthread_create():%s", strerror(err));
		return -err;
	}
	thr_channel[tid_nextpos].chnid = ptr->chnid;
	tid_nextpos++;
	return 0;
}

int thr_channel_destory(mlib_listentry_t * ptr)
{
	
	for(int i = 0; i < CHNNR; ++i)
	{	
		if(thr_channel[i].chnid == ptr->chnid)
		{
			if(pthread_cancel(thr_channel[i].tid) < 0)
			{
				syslog(LOG_ERR,"pthread_cancel()): the thread of channel %d", ptr->chnid);
				return -ESRCH;
			}	
			pthread_join(thr_channel[i].tid, NULL);
			thr_channel[i].chnid = -1;
			return 0;
		}
	}
	return -1;
}

int thr_channel_destory_all()
{
	for(int i = 0; i < CHNNR; ++i)
	{
		if(pthread_cancel(thr_channel[i].tid) < 0)
		{
			syslog(LOG_ERR,"pthread_cancel()): the thread of channel %d", thr_channel[i].chnid);
			return -ESRCH;
		}	
		pthread_join(thr_channel[i].tid, NULL);
		thr_channel[i].chnid = -1;
		
	
	}
	return 0;
}
