

#include<iostream>
#include<unistd.h>
#include<pthread.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<syslog.h>
#include<arpa/inet.h>
#include"server_conf.h"
#include"thr_list.h"
static pthread_t tid_list;
static int nr_list_ent;
static mlib_listentry_t *list_ent;
struct sockaddr_in sndaddr; 


static void*thr_list(void *p)
{
	int totalsize = sizeof(chnid_t);
	msg_list_t *entlistp;
	msg_listentry_t *entryp;

	for(int i = 0 ; i < nr_list_ent; ++i)
	{
		totalsize += sizeof(msg_listentry_t) + strlen(list_ent[i].desc);

	}

	entlistp = (msg_list_t*)malloc(totalsize);
	if(entlistp  == NULL)
	{
		syslog(LOG_ERR, "malloc(): %s", strerror(errno));
	} 

	entlistp->chnid = LISTCHNID;
	entryp = entlistp->entry;

	for(int i = 0; i < nr_list_ent; ++i)
	{
		int size = sizeof(msg_listentry_t) + strlen(list_ent[i].desc);

		entryp->chnid = list_ent[i].chnid;
		entryp->len = htons(size);
		strcpy((char*)entryp->desc,list_ent[i].desc);
		entryp = (msg_listentry_t*)(((char*)entryp) + size);
	}

	struct timeval timeout = {\
				.tv_sec = 1,
				.tv_usec = 0,
				};

	msg_list_t* tmptr = entlistp;
	//for(int i = 0; i < nr_list_ent; ++i)
	//{
	//	int size = sizeof(msg_listentry_t) + strlen(list_ent[i].desc);
	//	syslog(LOG_DEBUG, "%d : %s", tmptr->entry->desc);
	//}
	int ret = 0;
	while(1)
	{
		ret = sendto(serversd, entlistp, totalsize, 0 , (struct sockaddr*)&sndaddr, sizeof(sndaddr));
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
		sleep(1);
		//	select(-1, NULL, NULL, NULL, &timeout);
		sched_yield();

	}
}



int thr_list_create(mlib_listentry_t *mliblist , int size)
{
	int err = 0;
	list_ent = mliblist;
	nr_list_ent = size;
	err = pthread_create(&tid_list, NULL, thr_list, NULL);
	if(err)
	{
		syslog(LOG_ERR, "pthread_create(): %s", strerror(err));
		return -1;
	}
	return 0;
}
int thr_list_destroy()
{
	pthread_cancel(tid_list);
	pthread_join(tid_list, NULL);
	return 0;
}

