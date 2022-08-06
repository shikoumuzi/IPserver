#include<stdio.h>
#include<stdlib.h>

#include<glob.h>
#include<unistd.h>
#include<syslog.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>

#include<Mtbf.h>
#include"server_conf.h"
#include"medialib.h"

#include<iostream>
#include<fstream>

#include<string>
#include<vector>
#include<cstring>
#include<deque>

#define PATHSIZE 1024
#define LINEBUFSIZE 1024;

namespace MUZI{
struct ChannelContext
{
	chnid_t chnid;
	char *desc;
	glob_t musicglob;
	int pos;
	int fd;
	off_t offset;
	Mtbf* tbf;	
};

server_conf_t serverconf = {
			.rcvport 	= DEFAULT_RCVPORT,
			.mgroup	 	= DEFAULT_MGROUP,
			.media_dir	= DEFAULT_MEDIADIR,
			.ifname		= DEFAULT_IF,
			.runmod		= RUN_DEAMON};



using channel_context_t = struct ChannelContext;

MPRIVATE(MediaLib):public MIpv4ServerProto
{
public:
	MediaLibMPrivate(int maxtokens)
	{
		this->channel.resize(CHNNR + 1, nullptr);
		this->tbf = new Mtbf(1, nullptr, maxtokens);
		this->tbf->TbfStart();
	}
	
	~MediaLibMPrivate()
	{
		for(auto& x : this->channel)
		{
			if(x != nullptr)
			{	
				globfree(&x->musicglob);
				free(x->desc);
				x->tbf = nullptr;
				delete x;
				x = nullptr;
			}
		}
		for(auto& x : this->listentrybuff)
		{
			free(x);
			x = nullptr;
		}
		this->tbf->Tbfstop();
		delete this->tbf;
	}
public:
	int getChannelPos(channel_context_t *me)
	{
		int i = 1;
		for(; i < CHNNR; ++i)
		{
			if(this->channel[i] == nullptr)
			{
				this->channel[i] = me;
				return i;
			}
		}
		errno = ENOSPC;
		return -ENOSPC;
	}

	channel_context_t* path2entry(const char *path, std::vector<std::string>& duff_format)
	{
		glob_t globres;
		char pathstr[PATHSIZE + 4];
		char linebuff[PATHSIZE + 4];
		strncpy(pathstr, path, PATHSIZE);
		strncat(pathstr,"/desc.txt",PATHSIZE);

		std::fstream fs_desc;
		fs_desc.open(pathstr, std::ios::in);
		if(!fs_desc.is_open())
		{
			syslog(LOG_INFO,"fopen(): %s is not a channel dir(Can't find desc.txt)",path);
			return NULL;
		}	
		fs_desc >> linebuff;	
		fs_desc.close();

		channel_context_t *me = new channel_context_t;
		if(me == NULL)
		{
			syslog(LOG_ERR,"malloc(): %s\n",strerror(errno));
			return NULL;
		}

		me->desc = strdup(linebuff);
		//字符串复制函数，在复制的同时开辟新的内存空间进行存放
		
		int globnu = -1;
		for(auto& x : duff_format)
		{
			memset(pathstr, '\0', PATHSIZE);
			strncpy(pathstr, path, PATHSIZE);
			strncat(pathstr, "/*.",PATHSIZE);
			strncat(pathstr, x.c_str(), PATHSIZE);
			if((globnu = glob(pathstr, GLOB_APPEND & (globnu < 0)?0:1, NULL, &me->musicglob)) != 0)
			{
				syslog(LOG_ERR,"%s is not a channel dir(Can't not find %s file or open failed), err = %s", 
						path, x.c_str(), strerror(errno));
				free(me);
				return NULL;
			}
			
		}
		
		me->pos 	= 0;
		me->offset 	= 0;
		me->fd 		= open(me->musicglob.gl_pathv[me->pos], O_RDONLY);
		if(me->fd < 0)
		{
			syslog(LOG_WARNING, "%s open error: %s", me->musicglob.gl_pathv[me->pos], strerror(errno));
			free(me);
		}
		me->tbf 	= this->tbf;
		me->chnid 	= this->getChannelPos(me);
		return me;
	}

	int open_next(chnid_t chnid)
	{
		if(++this->channel[chnid]->pos == this->channel[chnid]->musicglob.gl_pathc)
		{
			lseek(this->channel[chnid]->fd, 0, SEEK_SET);
			this->channel[chnid]->pos = 0;
		}

		close(this->channel[chnid]->fd);
		int tmpfd = open(this->channel[chnid]->musicglob.gl_pathv[this->channel[chnid]->pos], O_RDONLY);
		if(tmpfd < 0)
		{
			syslog(LOG_WARNING, "open(%s):%s failed ", this->channel[chnid]->musicglob.gl_pathv[this->channel[chnid]->pos], strerror(errno));
		}
		else
		{
			lseek(this->channel[chnid]->fd, 0, SEEK_SET);
			this->channel[chnid]->offset = 0;
			this->channel[chnid]->fd = tmpfd;

		}
		return 0;
	}
public:
	std::vector<channel_context_t*> channel;
	std::vector<mlib_listentry_t*> listentrybuff;
	Mtbf* tbf;
};


//channel_context_t channel[CHNNR + 1];

MediaLib::MediaLib(int maxtokens)
{
	this->d = new MPRIVATE(MediaLib)(maxtokens);
}
MediaLib::~MediaLib()
{
	
	delete this->d;
}


int MediaLib::getchnlist(mlib_listentry_t **result, int *resnum, std::vector<std::string>& duff_format)
{
	std::string path = serverconf.media_dir; 
	path += "/*";
	glob_t globres;
	int num = 0;
	
	if(glob(path.c_str(), 0, NULL, &globres) != 0)
	{
		return -1;
	}	
	
	mlib_listentry_t *ptr = (mlib_listentry_t*)malloc(sizeof(mlib_listentry_t) * globres.gl_pathc);
	if(ptr == NULL)
	{
		syslog(LOG_ERR, "malloc failed");
		return -1;
	}

	channel_context_t *res = nullptr;
	for(int i = 0; i < globres.gl_pathc; ++i)
	{
		res = this->d->path2entry(globres.gl_pathv[i], duff_format);
		if(res != NULL)
		{
			syslog(LOG_DEBUG,"path2entry(): return %d:%s", res->chnid, res->desc);
			ptr[num].chnid	= res->chnid;
			
			
			char *tmp = strdup(res->desc);
			if(tmp == NULL)
			{
				
				syslog(LOG_DEBUG, "strdup(): err %s", strerror(errno));
			}
			
			ptr[num].desc 	= new std::string(tmp);
			//new 可以将类内包含的所有其他类对象都一并调用构造函数
			
			
			++num;
		}
		
	}	
	
	*resnum = num;
	*result	= (mlib_listentry_t*)realloc(ptr, sizeof(mlib_listentry_t) * (num));
	
	if(*result == NULL)
	{
		syslog(LOG_ERR, "realloc(): %s", strerror(errno));
		return -1;
	}
	
	this->d->listentrybuff.push_back(ptr);
	ptr = nullptr;
	globfree(&globres);
	
	
	return 0;

}



ssize_t MediaLib::readchnl(chnid_t chnid, void * buf, size_t size)
{
	int tbfsize = 0;
	tbfsize = this->d->channel[chnid]->tbf->getVaildTokens(1);
			
	syslog(LOG_DEBUG, "channel[%d].tbf:gettoken(%d)", chnid, tbfsize);	
	int len = 0;
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	while(1)
	{
		if((len = pread(this->d->channel[chnid]->fd, buf, tbfsize * size, 
						this->d->channel[chnid]->offset)) < 0)
		{
			syslog(LOG_WARNING, "media file %s pread(): %s",
				       	this->d->channel[chnid]->musicglob.gl_pathv[this->d->channel[chnid]->pos], strerror(errno));
			this->d->open_next(chnid);
		}
		else if(len == 0)
		{
			syslog(LOG_DEBUG, "thread id:%d, media file %s is over",
					pthread_self(), this->d->channel[chnid]->musicglob.gl_pathv[this->d->channel[chnid]->pos]);
			this->d->open_next(chnid);
		}
		else
		{
			this->d->channel[chnid]->offset += len;
			break;
		}
		select(0, NULL, NULL, NULL, &timeout);
	}

	if(tbfsize - len  > 0)
	{
		this->d->channel[chnid]->tbf->pushTokens(tbfsize);

	}
	return len;
}

};
