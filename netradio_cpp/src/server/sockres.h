#ifndef __MUSOCKRES_H__
#define __MUSOCKRES_H__

namespace MUZI{
class SockRes
{
public:
	SockRes();
	SockRes(SockRes& object);
	~SockRes();
public:
	int SockInit();
protected:

	void setSockData(SockRes& object);
	SockRes getSockRes();
	int getSockfd();
	void* getSockaddr();
	void* getIpMreqn();
protected:
	MPRIVATE(SockRes) *sockdata;
};
};
#endif
