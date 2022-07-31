#include<stdio.h>
#include<stdlib.h>
#include<glob.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<syslog.h>
#include<Mtbf.h>
#include"server_conf.h"
#include"medialib.h"

#define PATHSIZE 1024
#define LINEBUFSIZE 1024;

typedef struct ChannelContext
{
	chnid_t chnid;
	char *desc;
	glob_t mp3glob;
	int pos;
	int fd;
	off_t offset;
	Mtbf* tbf;	
}channel_context_t;

channel_context_t channel[CHNNR + 1];

int getGlobPos(const char* path, glob_t* glob)
{
	int i = 0;
	for(; i < glob->gl_pathc; ++i)
	{
		if(strstr(glob->gl_pathv[i], path) != NULL)
		{
			return i;
		}
	}
	return -1;
}

static channel_context_t* path2entry(const char *path)
{
	glob_t globres;
	char pathstr[PATHSIZE + 4];
	char linebuff[PATHSIZE + 4];
	strncpy(pathstr, path, PATHSIZE);
	strncat(pathstr,"/desc.txt",PATHSIZE);
	static chnid_t curr_id = MINCHNID;

	FILE* fp = fopen(pathstr,"r");
	if(fp == NULL)
	{
		syslog(LOG_INFO,"fopen(): %s is not a channel dir(Can't find desc.txt)",path);
		return NULL;
	}	

	if(fgets(linebuff, PATHSIZE, fp) == NULL )
	{
		syslog(LOG_INFO,"fgets(): %s is not a channel dir(Can't find desc.txt)",path);
		fclose(fp);
		return NULL;
	}	
	fclose(fp);

	channel_context_t *me = (channel_context_t*)malloc(sizeof(channel_context_t));
	if(me == NULL)
	{
		syslog(LOG_ERR,"malloc(): %s\n",strerror(errno));
		return NULL;
	}

	me->desc = strdup(linebuff);
	//字符串复制函数，在复制的同时开辟新的内存空间进行存放
	memset(pathstr, '\0', PATHSIZE);
	strncpy(pathstr, path, PATHSIZE);
	strncat(pathstr,"/*.mp3", PATHSIZE);
	
	int globres_mp3 = 0, globres_flac = 0;
	globres_mp3 = glob(pathstr, 0, NULL, &me->mp3glob);
	
	memset(pathstr, '\0', PATHSIZE);
	strncpy(pathstr,path, PATHSIZE);
	strncat(pathstr,"/*.flac", PATHSIZE);
	
	globres_flac = glob(pathstr, GLOB_APPEND, NULL,&me->mp3glob);
	
	if(globres_flac != 0 && globres_mp3 != 0)		
	{	
		curr_id ++ ;
		syslog(LOG_ERR,"%s is not a channel dir(Can't find mp3 files)", path);
		free(me);
		return NULL;
	}
	
	for(int i = 0; i < globres.gl_pathc && globres.gl_pathv + i != nullptr; ++i)
	{
		syslog(LOG_DEBUG, "%s\n", globres.gl_pathv[i]);
	}

	me->pos = 0;
	me->offset = 0;
	me->fd = open(me->mp3glob.gl_pathv[me->pos], O_RDONLY);
	if(me->fd < 0)
	{
		syslog(LOG_WARNING, "%s open error: %s", me->mp3glob.gl_pathv[me->pos], strerror(errno));
		free(me);
	}
	me->tbf = new Mtbf(1, nullptr, 2* MSG_DATA_MAX * 2);
	me->chnid = curr_id++;
	me->tbf->TbfStart();
	
	return me;
}


int mlib_getchnlist(mlib_listentry_t **result, int *resnum)
{
	char path[PATHSIZE];
	glob_t globres;
	int num = 0;

	mlib_listentry_t *ptr;
	channel_context_t *res;
	


	for(int i = 0; i < MAXCHNID + 1; ++i)
	{
		channel[i].chnid = -1;
	}
	snprintf(path, PATHSIZE, "%s/*", serverconf.media_dir);
	

	if(glob(path, 0, NULL, &globres) != 0)
	{
		return -1;
	}
	
	ptr = (mlib_listentry_t*)malloc(sizeof(mlib_listentry_t) * globres.gl_pathc);
	if(ptr == NULL)
	{
		syslog(LOG_ERR, "malloc failed");
		return -1;
	}
	for(int i = 0; i < globres.gl_pathc; ++i)
	{
		res = path2entry(globres.gl_pathv[i]);
		if(res != NULL)
		{
			syslog(LOG_DEBUG,"path2entruy(): return %d:%s", res->chnid, res->desc);
			memcpy(channel + res -> chnid, res, sizeof(*res));
			ptr[num].chnid = res->chnid;
			ptr[num].desc = strdup(res->desc);
			++num;
		}
		
	}	
	
	*resnum = num;
	*result	= (mlib_listentry_t*)realloc(ptr, sizeof(mlib_listentry_t) * num);
	if(*result == NULL)
	{
		syslog(LOG_ERR, "realloc(): %s", strerror(errno));
		return -1;
	}


		
	return 0;

}
int mlib_freechnlist(mlib_listentry_t* mliblistentry)
{
	free(mliblistentry);
	return 0;
}

static int open_next(chnid_t chnid)
{
	if(++channel[chnid].pos == channel[chnid].mp3glob.gl_pathc)
	{
		lseek(channel[chnid].fd, 0, SEEK_SET);
		channel[chnid].pos = 0;
	}

	close(channel[chnid].fd);
	int tmpfd = open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos], O_RDONLY);
	if(tmpfd < 0)
	{
		syslog(LOG_WARNING, "open(%s):%s failed ", channel[chnid].mp3glob.gl_pathv[channel[chnid].pos], strerror(errno));
	}
	else
	{
		lseek(channel[chnid].fd, 0, SEEK_SET);
		channel[chnid].offset = 0;
		channel[chnid].fd = tmpfd;

	}
	return 0;
}

ssize_t mlib_readchnl(chnid_t chnid, void * buf, size_t size)
{
	int tbfsize = 0;
	tbfsize = channel[chnid].tbf->getVaildTokens(1);
			
	syslog(LOG_DEBUG, "channel[%d].tbf:gettoken(%d)", chnid, tbfsize);	
	int len = 0;
	while(1)
	{
		if((len = pread(channel[chnid].fd, buf, tbfsize * size, channel[chnid].offset)) < 0)
		{
			syslog(LOG_WARNING, "media file %s pread(): %s", channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
			open_next(chnid);
		}
		else if(len == 0)
		{
			syslog(LOG_DEBUG, "thread id:%d, media file %s is over",pthread_self(), channel[chnid].mp3glob.gl_pathv[channel[chnid].pos]);
			open_next(chnid);
		}
		else
		{
			channel[chnid].offset += len;
			break;
		}
		sleep(1);
	}

	if(tbfsize - len  > 0)
	{
		channel[chnid].tbf->pushTokens(tbfsize);

	}
	return len;
}


