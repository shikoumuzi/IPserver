#ifndef __MUTHR_LIST_H_
#define __MUTHR_LIST_H_

#include"medialib.h"
namespace MUZI{

class ThrMediaList:public MIpv4ServerProto{
public:
	friend void operatordelete(ThrMediaList* object);
	static ThrMediaList* getThrMediaList(SockRes *sockres)
	{
		return new ThrMediaList(sockres);
	}
protected:
	ThrMediaList(SockRes *sockres);
	ThrMediaList(const ThrMediaList& object) = delete;
public:
	~ThrMediaList();		
	int create(mlib_listentry_t *mliblist , int size);

protected:
	MPRIVATE(ThrMediaList) *d;
};
};
#endif
