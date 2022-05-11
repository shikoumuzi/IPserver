#include<iostream>

#define MDATA(classname) class classname##Mdata
#define MMAXTOKENS 	1024

MDATA(Mtbf);

class Mtbf
{
public:
	friend struct MGroupBase;
public:
	Mtbf();
	Mtbf(int s, int* pos, int token = MMAXTOKENS );
	~Mtbf();
	int TbfStart();
	int Tbfstop();
	int getVailTokens(int s);
	int pushTokens(int tokens);
	int destoryToken();
		

private:
	void pushTbf(void *p);
	MDATA(Mtbf) *d;
};
