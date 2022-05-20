#include<iostream>

#define MDATA(classname) class classname##Mdata
#define MMAXTOKENS 	1024

MDATA(Mtbf);
MDATA(MtbfGroup);
class Mtbf;
class MtbfGroup;

struct PrivateData
{
	void *data;
};

class Mtbf
{
public:
	friend struct MGroupBase;
	friend class MtbfGroup;
	friend MDATA(MtbfGroup);
public:
	Mtbf();
	Mtbf(int s, int* pos, int token = MMAXTOKENS );
	~Mtbf();
	int TbfStart();
	int Tbfstop();
	void createTbf(int s, int *pos, int token = MMAXTOKENS);
	int getVaildTokens(int s);
	int pushTokens(int tokens);
	int destoryToken();
		

private:
	void pushTbf(void *p);
	MDATA(Mtbf) *d;
	struct PrivateData* Pdata;
};

class MtbfGroup
{
public:
	MtbfGroup();
	~MtbfGroup();
	Mtbf& operator[](int pos);
	void pushTbf();
	int getPos(Mtbf*tbf);
	int isFull();
	int isMember(Mtbf* tbf);
	int destroyTbf(int pos);	
public:
	MDATA(MtbfGroup) *d;
};
