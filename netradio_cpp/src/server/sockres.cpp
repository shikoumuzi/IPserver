
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<ipv4serverproto.h>


#include"server_conf.h"
#include"sockres.h"
#include<string.h>
#include<string>
#include<syslog.h>
namespace MUZI{

struct SockFdRes
{
	int sockfd;
	int count;
};
MPRIVATE(SockRes)
{
public:
	int* socksd;
	struct sockaddr_in sndaddr;
	struct ip_mreqn mreqn;
	struct SockFdRes sockfdres;
};

SockRes::SockRes():sockdata(new MPRIVATE(SockRes))
{
	this->sockdata->socksd 			= &this->sockdata->sockfdres.sockfd;
	this->sockdata->sockfdres.count 	= 1;
}
SockRes::SockRes(SockRes& object):sockdata(new MPRIVATE(SockRes))
{
	memcpy(this->sockdata, object.sockdata, sizeof(*(this->sockdata)));
	this->sockdata->sockfdres.count += 1;
}
SockRes::~SockRes()
{
	
	if(--this->sockdata->sockfdres.count == 0)
		close(*this->sockdata->socksd);
	
	delete this->sockdata;
	
}

int SockRes::SockInit()
{
	int socksd = 0;
	if((socksd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		syslog(LOG_ERR, "ThrMediaList::ThrMediaList():socket(): %s", strerror(errno));
		throw std::string("ERR_SOCK");
		return -errno;
	}

	*this->sockdata->socksd 		= socksd;
	inet_pton(AF_INET, serverconf.mgroup, &this->sockdata->mreqn.imr_multiaddr);
	inet_pton(AF_INET, "0.0.0.0", &this->sockdata->mreqn.imr_address);
	this->sockdata->mreqn.imr_ifindex	= if_nametoindex(serverconf.ifname);
	
	int rt = 0;
	if((rt = setsockopt(socksd, IPPROTO_IP, IP_MULTICAST_IF, 
				&(this->sockdata->mreqn), sizeof(this->sockdata->mreqn))) == -1)
	{
		syslog(LOG_ERR, "ThrMediaList::ThrMediaList():setssockopt(): %s", strerror(errno));
		
		
		
		throw std::string("ERR_SOCK");	
		return -errno;
	}
	this->sockdata->sndaddr.sin_family 	= AF_INET;
	this->sockdata->sndaddr.sin_port 	= htons(atoi(serverconf.rcvport));
	inet_pton(AF_INET, "0.0.0.0", &this->sockdata->sndaddr.sin_addr);

	return 0;
}
void SockRes::setSockData(SockRes& object)
{
	memcpy(this->sockdata, object.sockdata, sizeof(*object.sockdata));
}
SockRes SockRes::getSockRes()
{
	return *this; 
}
int SockRes::getSockfd()
{
	return *this->sockdata->socksd;
}

void* SockRes::getSockaddr()
{
	return &this->sockdata->sndaddr;
}
void* SockRes::getIpMreqn()
{
	return &this->sockdata->mreqn;
}
};
