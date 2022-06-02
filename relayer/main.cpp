#include"relayer.h"
#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<signal.h>
using namespace std;

#define TTY1 "/dev/tty11"
#define TTY2 "/dev/tty12"

int main()
{
	int fd1 = open(TTY1, O_RDWR | O_NONBLOCK);
       	int fd2 = open(TTY2, O_RDWR | O_NONBLOCK);
	MRJobGroup mrjobs; 
	int rd = mrjobs.MRelAddJob(fd1, fd2);
	write(fd1, "TTY1\n" , 5);
	write(fd2,"TTY2\n", 5);
	
	int sig;
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigwait(&set, &sig);

	mcout("work end");
	mrjobs.MRelCanelJob(rd);
	mrjobs.MRelWaitJob(rd, nullptr);
	close(fd1);
	close(fd2);
	return 0;
}
