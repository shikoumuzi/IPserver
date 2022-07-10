#include<stdio.h>
#include<stdlib.h>
#include"sockpropool.h"
using namespace std;
int main()
{
	MSockPro* sockpool = MSockProInit("0.0.0.0");	
       	//cout<< sockpool->d->sd <<" "<< sockpool->d->net<< " "
	//	<< sockpool->d->inetproto <<" " << sockpool->d->port<< endl;
		
	sockpool->MDriverPool();
	return 0;
}
