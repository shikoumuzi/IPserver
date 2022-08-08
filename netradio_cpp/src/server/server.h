#ifndef __MUSERVER_H__
#define __MUSERVER_H__
#include<ipv4serverproto.h>
#include<vector>
#include<string>
#include"sockres.h"
namespace MUZI{
class Server:public SockRes
{
public:
	static Server* getServer(int argc, char* argv[])
	{
		return new Server(argc, argv);
	}
protected:
	Server(int argc, char* argv[]);
public:
	~Server();
	int SigInit(std::vector<int>& signum);
	int analyseOpt(int argc, char* argv[]);
	int assignFormat(std::vector<std::string>& duffforamt);
	int getChnlList();
	int createListThr();
	int createChnlThr();
protected:
	MPRIVATE(Server) *d;
};

};
#endif
