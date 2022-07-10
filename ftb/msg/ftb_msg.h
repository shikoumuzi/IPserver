#include<sys/types.h>
#include<sys/ipc.h>
#include"protocol.h"

#define MDATA(classname) class classname##Mdata
#define MMSGMAX 64


#define MMSG_PRIVATE  	0X00000000UL
#define MMSG_PUBLIC	0x00000001UL
#define MMSG_AUTO	0x00000002UL
#define MMSG_SEND	0x00000004UL
#define MMSG_RCV	0x00000008UL
#define MMSG_KPSTAT	0x00000010UL
#define MMSG_CHSTAT	0X00000020UL

#define MMSGDATA	0x00000001UL

MDATA(MMsg);

template<typename T>
void mcout(T data)
{
	std::cout<< data << std::endl;
}

class MMsg
{
public:
	MMsg();
	~MMsg();
public:
	void operator=(MMsg&& othermsg);
public:
	int MMsgRegister(u_int64_t mod, const char* pathname);
	
	int MMsgCtl(int md, u_int64_t mod,  void* data, void* retdata , long size, long mtype = 0);
	int MMsgDestroy(int md);
public:
	MDATA(MMsg) *d;
};

