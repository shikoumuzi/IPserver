#ifndef SOCKPROPOOL
#define SOCKPROPOOL

#include<string>
#include<sys/mman.h>
#include"proto.h"


#define MINSPARESERVER 	5
#define MAXSPARESERVER 	20
#define MAXCLIENTS	100
#define SIG_NOTIFY	SIGUSR2
#define MPRIVATE(classname) class classname##MPrivate

#define MINET 		0x00000001UL
#define MINET6	 	0x00000002UL

#define MTCP 		0x00000001UL
#define MUDP 		0x00000002UL

#define MSTATE_IDLE	0x00000001UL
#define MSTATE_BUSY	0x00000002UL

#define MAXDATASPACE	512
#define SOCKPOOLPATHNAME "."

struct data_format
{
	char data[MAXDATASPACE];
};

class MSockPro
{
public:
	
	static void MSockProWait(int s);
	static void propoolfun(void* data)
	{
		struct data_format* dataptr = (struct data_format*)data;
		printf("%s", dataptr->data);
	}
public:
	MSockPro():d(nullptr){};
	void MSockProInit(std::string ipstr,
			void (*datactl)(void *),
			u_int64_t net,
			int inetproto,	
			int port, 
			int minspareserver,
			int maxspareserver,
			int maxclient);
	int MDriverPool();
	int MSendData(const void *buff, u_int64_t size);
	int MRecvData(void *buff, u_int64_t size);	
	~MSockPro();

public:
	MPRIVATE(MSockPro)* d;
};

MSockPro* MSockProInit(std::string ipstr,
			void(*datactl)(void *) = MSockPro::propoolfun,
                        u_int64_t net = MINET,
                        int inetproto = MTCP,
                        int port = atoi(PORT),
                        int minspareserver = MINSPARESERVER,
                        int maxspareserver = MAXSPARESERVER,
                        int maxclient = MAXCLIENTS);
#endif
