#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/types.h>
#include"Mtbf.h"
using namespace std;

#define FILENAME "./Mtbf.cpp"
int main()
{

	int fd = open(FILENAME, O_RDONLY);
	char buff[125];

	int len = 0;
	int toks = 0;

	Mtbf tbf(1, nullptr);
	tbf.TbfStart();
	while(toks = tbf.getVaildTokens(10))
	{
		for(int i = 0; i < toks; ++i)
		{		
			len = read(fd, buff, 10);
			write(1, buff, len);
		}
		tbf.pushTokens(toks);
		sleep(1);
	}
	close(fd);
	
	
	return 0;
}
