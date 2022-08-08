#include<iostream>
#include<signal.h>
#include"server.h"
#include<unistd.h>

using namespace std;
using namespace MUZI;

int main(int argc, char* argv[])
{

	Server* server = Server::getServer(argc, argv);

	vector<string> duffformat;
	duffformat.push_back(string("mp3"));
	
	std::vector<int> sig;
	sig.push_back(SIGINT);
	server->SigInit(sig);

	server->assignFormat(duffformat);

	server->getChnlList();

	server->createListThr();

	server->createChnlThr();


	while(1)
		pause();
}
