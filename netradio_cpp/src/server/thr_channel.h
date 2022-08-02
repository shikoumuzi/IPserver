#ifndef __MUTHR_CHANNEL_H__
#define __MUTHR_CHANNEL_H__

#include"medialib.h"

namespace MUZI{

class ThrMediaChannel:MIpv4ServerProto{
public:
	ThrMediaChannel(int sd, struct sndaddr& sndaddr);
	ThrMediaChannel(int sd, struct ip_mreqn* mreqnrt, struct sndaddr* sndaddrrt);
	~ThrMediaChannel();
public:
	int create(mlib_listentry_t* chnllist);
	int destroyonce(mlib_listentry_t* chnllist);
	int destroy_all();
protected:
	MPRIVATE(ThrMediaChannel)i *d;
};
};
#endif
