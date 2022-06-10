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
	pack.data.math 		= 0;
	pack.data.chinese 	= 0;	
	memcpy(&retpack, &pack, sizeof(pack));
	mcout("receiver.cpp: pack create success\n");	
	
	fmsg.MMsgCtl(md, MMSG_RCV | MMSG_KPSTAT, &pack.data, &retpack.data, sizeof(retpack.data), MMSGDATA);
	
	cout<<	"retpack.mtype: " << retpack.mtype 
		<< "\nretpack.data.math: " << retpack.data.math
	       	<< "\nretpack.data.chinese: " << retpack.data.chinese <<endl;
	
	
	return 0;
}
