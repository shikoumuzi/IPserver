#include<iostream>
#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include"protocol.h"
#include"ftb_msg.h"
using namespace std;

struct rdata
{
	char name[100];
	int math;
	int chinese;
};


int main(int arg, char*argv[])
{
	MMsg fmsg;
	int md = fmsg.MMsgRegister(MMSG_PUBLIC | MMSG_SEND, MPATHNAME);
	mcout("register");

	msg_data_t<rdata> pack, retpack;
	pack.mtype 		= MMSGDATA;
	memcpy(pack.data.name , argv[1],strlen(argv[1]));
	pack.data.math 		= 100;
	pack.data.chinese 	= 20;	
	
	mcout("ctl");
	fmsg.MMsgCtl(md, MMSG_SEND | MMSG_KPSTAT, &pack, &retpack, sizeof(pack) - sizeof(long), MMSGDATA);
/*	
	cout<<	"pack.mtype: " <<pack.mtype 
		<< "\npack.data.math: " << pack.data.math
	       	<< "\npack.data.chinese: " << pack.data.chinese <<endl;

*/	
	cout<< "OK!" <<endl;	
	return 0;
}
