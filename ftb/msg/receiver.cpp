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


int main()
{
	MMsg fmsg;
	u_int64_t mod = 0;
	int md = fmsg.MMsgRegister((MMSG_PUBLIC | MMSG_RCV ), MPATHNAME);
	
	msg_data_t<rdata> pack, retpack;
	pack.mtype 		= 0;
	memcpy(pack.data.name, "shikoumuzi", sizeof("shikoumuzi"));
	pack.data.math 		= 0;
	pack.data.chinese 	= 0;	
	memcpy(&retpack, &pack, sizeof(pack));
	mcout("receiver.cpp: pack create success\n");	
	
	fmsg.MMsgCtl(md, MMSG_RCV | MMSG_KPSTAT, &pack, &retpack, sizeof(retpack) - sizeof(long), MMSGDATA);
	
	cout<<	"retpack.mtype: " << retpack.mtype
	       << "\nretpack.data.name: "<< retpack.data.name	
		<< "\nretpack.data.math: " << retpack.data.math
	       	<< "\nretpack.data.chinese: " << retpack.data.chinese <<endl;
	fmsg.MMsgDestroy(md);	
	
	return 0;
}
