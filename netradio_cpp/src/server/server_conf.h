#ifndef __MUSERVER_CONF_H__
#define __MUSERVER_CONF_H__


#define DEFAULT_MEDIADIR	"/var/media/"
#define DEFAULT_IF		"eth0"

namespace MUZI{
enum{
	RUN_DEAMON = 0x00000001UL,
	RUN_FOREGROUND

};



typedef struct ServerConf
{
	char *rcvport;
	char *mgroup;
	char *media_dir;
	char *ifname;
	char runmod;


}server_conf_t;


extern server_conf_t serverconf;
extern int serversd;
extern struct sockaddr_in sndaddr;
};
#endif
