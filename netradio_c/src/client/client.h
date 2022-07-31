#ifndef __MUCLIENT_H__
#define __MUCLIENT_H__

#include<iostream>
#include<stdio.h>
#include<ipv4serverproto.h>
#define __MUDEFAULT_PLAYERCMD__ "mplayer -  > /dev/null"
//将mpg123的文字输出到空设备上

typedef struct ClientConfST
{

	char *rcvport;
	char *mgroup;
	char *player_cmd;
}client_conf_st;

static client_conf_st client_conf = {\
	.rcvport = DEFAULT_RCVPORT,\
	.mgroup  = DEFAULT_MGROUP,\
	.player_cmd = __MUDEFAULT_PLAYERCMD__};

static void ClientHelpPrintf()
{
	std::cout
		<< "*-M --mgroup: 指定多播组\n"
 		<< "*-P --port: 指定接收端口\n"
 		<< "*-p --player: 指定解码器\n"
 		<< "*-H --help: 帮助\n"
		<< std::endl;
}



#endif
