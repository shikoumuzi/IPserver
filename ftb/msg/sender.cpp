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
	int md = fmsg.MMsgRegister(MMSG_PUBLIC | MMSG_SEND, MPATHNAME);
	
	msg_data_t<rdata> pack, retpack;
	pack.mtype 		= MMSGDATA;
	//memcpy(pack.data.name , "shikoumuzi",strlen("shikoumuzi"));
	pack.data.math 		= 100;
	pack.data.chinese 	= 20;	
	
	fmsg.MMsgCtl(md, MMSG_SEND | MMSG_KPSTAT, &pack.data, &retpack.data, sizeof(pack.data), MMSGDATA);
/*	
	cout<<	"pack.mtype: " <<pack.mtype 
		<< "\npack.data.math: " << pack.data.math
	       	<< "\npack.data.chinese: " << pack.data.chinese <<endl;
	
*/
	cout<< "OK!" <<endl;	
	return 0;
}
