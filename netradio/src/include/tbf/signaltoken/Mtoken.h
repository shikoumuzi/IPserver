#include<iostream>
#include<vector>
#include<signal.h>

#define MDATA(classname) class classname##MDATA
#define  MMAXINGROUP	1024
#define  MTA		(size_t)(-1)	

typedef void (*sighandler_t)(int);
static void MSignalHandler(int s, siginfo_t* info, void *p);
static void MSignalHandlerForGroup(int s, siginfo_t* info, void *p);

MDATA(Mtoken);
MDATA(MtokenGroup);
class Mtoken;
class MtokenGroup;

class Mtoken
{
private:
	Mtoken();
public:
	Mtoken(int sec,int64_t token = 1024, int pos = -1);
	~Mtoken();
public:

	friend void MSignalHandlerForGroup(int s, siginfo_t * info, void *p);
	friend void MSignalHandler(int s, siginfo_t* info, void* p);
	friend void sighandler(int s);
	friend struct MsigToken;
	friend class MtokenGroup;
	friend MDATA(MtokenGroup);
public:
	bool TokenStart(int signum = SIGALRM);
	sigset_t getMask();
	void changeMask(const sigset_t *mask);
	int getVaildToken(); 
	bool TokenDestory();
	bool TokenEnd();
	
private:
//	friend class MtokenGroup;
//	friend MDATA(MtokenGroup);
	MDATA(Mtoken) *d;
};

class MtokenGroup
{
public:
	friend void MSignalHandlerForGroup(int s, siginfo_t* info, void *p);
public:
	MtokenGroup();
	MtokenGroup(MtokenGroup&& othergroup);
	~MtokenGroup();
	void createMtokenGroup();
public:
	int autoToken(int sec = 1, int64_t token = MMAXINGROUP);
	int pushToken(Mtoken** token, bool ishasp = false);
	int delToken(int pos);
	Mtoken& operator[](int pos);
	int JudGroupPos(int pos);
	int TokenStart(int pos);//if input value is MTA, this function will start all
	int TokenEnd(int pos);// if input valur is MTA, this function will delete all token
public:
	MDATA(MtokenGroup) *d;
};
