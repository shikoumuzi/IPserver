#include<iostream>
#include<sys/types.h>
#include<unistd.h>
#include<stdlib.h>
#include"MMutex.h"

using namespace std;

int main()
{
	MMutex mut;
	int mtid = mut.getLock();

	int i = 0;
	int pid = fork();

	if(pid == 0)
	{
		while(1)
		{
			mut.Lock(mtid);
				cout<<"chid pro is locking"<<endl;
			
			sleep(1);
			mut.unLock(mtid);
		}
		
		exit(0);
	}	
	cout<<"parent"<<endl;
	while(1)
	{
		mut.Lock(mtid);
			cout<< "parent pro is locking"<<endl;
			//sleep(1);
		mut.unLock(mtid);
	}

	return 0;
}
