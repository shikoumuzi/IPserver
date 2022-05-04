#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include"Mtoken.h"
#include<sys/stat.h>
#include<unistd.h>
#include<glob.h>
#include<errno.h>
#include<sys/types.h>
#include<fcntl.h>

char *name = nullptr;
int fd;
int buf[1024];
int rt;
int max = 8*10;




typedef void (*sighandler_t)(int);


int main(int argc, char **argv)
{
	if(argc < 1)
	{
		fprintf(stderr, "Usage commend....");
		exit(1);
	}	
	name = argv[1];
	fd = open(name,  O_RDWR );
	rt = 0;

	Mtoken token(5);
	token.TokenStart();
	

	int tokens = 0;
	while((tokens = token.getVaildToken()))
	{
		for(int i = 0; i < tokens; ++i )
		{
			if( (rt = read(fd, buf, max)) > 0)
			{
				write(1,buf,rt);
			}
		}
	}
			
	close(fd);
	return 0;
}
