#ifndef __MUMEDIALIB_H__
#define __MUMEDIALIB_H__
#include<ipv4serverproto.h>
#include<string>
#include<vector>
namespace MUZI{

struct MlibListEntry
{
	chnid_t chnid;
	std::string desc;
};

using mlib_listentry_t = struct MlibListEntry;
class MediaLib:public MUZI::MIpv4ServerProto
{
public:
	static MediaLib* getMediaLib(int maxtokens = MSG_DATA_MAX)
	{
		return new MediaLib(maxtokens);
	}
public:
protected:
	MediaLib(int maxtokens);
	MediaLib(const MediaLib& object) = delete;
	~MediaLib();
public:
	int getchnlist(mlib_listentry_t **result, int *resnum, std::vector<std::string>& duff_format);	
	ssize_t readchnl(chnid_t chnid, void * buff, size_t size);
protected:
	MPRIVATE(MediaLib) *d;

};
};
#endif
