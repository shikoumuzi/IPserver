#ifndef MMUTEX
#define MMUTEX

#define MDATA(classname) class classname##Mdata
#define MPATHNAME 	"../sem"
#define MMUTEXSMAX 	1024
#define MPROJ		'a'

class autoLock;

class MMutex
{
public:
	MMutex();
	~MMutex();
public:
	int getLock();
	int Lock(int mtid);
	int unLock(int mtid);
	class autoLock autoLock();
	int Lockable(int mtid);
	bool LockStat(int mtid);
private:
	MDATA(MMutex) *d;
};





#endif
