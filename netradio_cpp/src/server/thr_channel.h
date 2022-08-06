#ifndef __MUTHR_CHANNEL_H__
#define __MUTHR_CHANNEL_H__

#include"medialib.h"
#include"sockres.h"
namespace MUZI{

class ThrMediaChannel:public MIpv4ServerProto, public SockRes
{
public:
	static ThrMediaChannel* getThrMediaChannel
					(MediaLib& media, SockRes *res)
	{
		return new ThrMediaChannel(media, res);
	}	
public:
	ThrMediaChannel(MediaLib& media, SockRes* sockres);
	~ThrMediaChannel();
public:
	int create(mlib_listentry_t* chnllist);
	int destroyonce(mlib_listentry_t* chnllist);
	int destroyall();
protected:
	MPRIVATE(ThrMediaChannel) *d;
};
};
#endif
