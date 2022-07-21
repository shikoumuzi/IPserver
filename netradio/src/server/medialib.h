#ifndef __MUMEDIALIB_H__
#define __MUMEDIALIB_H__
#include<ipv4serverprot.h>


typedef struct MlibListEntry
{
	chnid_t chnid;
	char *desc;

}mlib_listentry_t;

int mlib_getchnlist(mlib_listentry_t **result, int *resnum);
int mlib_freechnlist(mlib_listentrt_t* mliblistentry);

int mlib_readchnl(chnid_t , void *, size_t )
#endif
