#include<stdio.h>
#include<stdlib.h>
#include<glob.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<threadtbf.h>
#include"medialib.h"

#define PATHSIZE 1024


typedef struct ChannelContext
{
	chnid_t chnid;
	char *desc;
	glob_t mp3glob;
	int pos;
	int descpos;
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

void path2entry(const char *path)
{
	glob_t globres;
	char pathbuff[PATHSIZE + 4];
	size_t pathsize = strlen(path);
	
	memcpy(pathbuff, path, pathsize);
	strcat(pathbuff, "/*");
	
	if(glob(pathbuff, 0, NULL, &globres) == 0)
	{
		int i = 0;
		for(; i < MAXCHNNR; ++i)
		{
			if(channel[i].chnnid == -1)
			{
				channel[i].mp3glob 	= globres;
				channel[i].pos 		= i;
				channel[i].offset	= 0;
				channel[i].tbf		= new Mtbf;
				channel[i].fd		= -1;
				break;
			}
		}
		int descfd = 0, descpos = getGlobPos("desc", &globres);
		if(descpos < 0)
		{
			return;
		}
		channel[i].descpos = descpos;
		if((descfd = open(globres.gl_path[descpos], O_RDONLY)) < 0)
		{
			syslog(LOG_WARNING, "open an false dir");
			return;
		}
		channel[i].desc = (char*)malloc(PATHNAME);
		if(read(descfd, channel[i].desc, PATHNAME) < 0);
		{
			syslog(LOG_WAENING,"open desc failed");
			return;
		}


	}



}

int mlib_getchnlist(mlib_listentry_t **result, int *resnum)
{
	char path[PATHSIZE];
	glob_t globres;
	int num = 0;

	mlib_libentry_t *ptr;
	channel_context_t *res;
	


	for(int i = 0; i < MAXCHNNR + 1; ++i)
	{
		channel[i].chnid = -1;
	}
	snprintf(path, PATHSIZE, "%s/*", server_conf.media_dir);
	

	if(glob(path, 0, NULL &globres) != 0)
	{
		return -1;
	}
	
	ptr = (mlib_libentry_t*)malloc(sizeof(mlib_libentry_t) * globres.gl_pathc);
	if(ptr == NULL)
	{
		syslog(LOG_ERR, "malloc failed");
		return -1;
	}
	for(int i = 0; i < globres.gl_pathc; ++i)
	{
		path2entry(globres.gl_pathv[i]);
		++num;
	}
	
	

	*resnum = num;
	*result	= ptr;



	return 0;

}
int mlib_freechnlist(mlib_listentrt_t* mliblistentry)
{

}


