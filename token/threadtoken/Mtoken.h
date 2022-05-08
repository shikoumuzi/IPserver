#include<iostream>

#define MDATA(classname) class classname##Mdata
#define MMAXTOKENS 	1024

class Mtoken
{
public:
	Mtoken();
	Mtoken(int s, int token = MAXTOKENS );
	~Mtoken();
	bool TokenStart();
	bool TokenEnd();
	int getVailTokens();
	int pushTokens(int tokens);
	int destoryToken();
		

private:
	MDATA(Mtoken) *d;
}
