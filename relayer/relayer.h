#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#define MDATA(classname) class classname##Mdata
#define JOBSMAX 1024
enum
{
	STATE_RUNNING = 0x00000001UL,
	STATE_CANCEL,
	STATE_OVER
};

template<typename T>
void mcout(T output)
{
	std::cout << output << std::endl;
}

struct MRelStatSt
{
	u_int64_t state;
	int fd1;
	int fd2;
	u_int64_t cout12, cout21;
	struct timeval startT, endT;
	bool isvaild:1;
};

class MRelayer
{
public:
	friend class MRJobGroup;
public:
	MRelayer(int sfd, int dfd);
	~MRelayer();
private:
	MDATA(MRelayer) *d;
	int rd;	
};

class MRJobGroup
{
public:
	MRJobGroup();
	~MRJobGroup();
public:
	int MRelAddJob(int fd1, int fd2);
	/*
	 * 	return  rd successed 
	 * 		-EIVNAL error arg
	 * 		-ENOSPC full of jobs
	 * 		-ENOMEM no memory
	 * */
	int MRelAddJob(MRelayer* sjob, MRelayer* djob);
	int MRelCanelJob(int rd);
	/*
	 * 	return  0 successed
	 * 		-EBUSY jobs already be caneled
	 * 		-EINVAL error arg
	 * */
	int MRelWaitJob(int rd, struct MRelStatSt* jobstat);
	/*
	 * 	return  0 successed
	 * 		-EINVAL err arg
	 * */
	int MRelJobStat(int rd, struct MRelStatSt* Jobstat);
	
	struct MRelStatSt operator[](int rd);
private: 
	MDATA(MRJobGroup) *d;
};
