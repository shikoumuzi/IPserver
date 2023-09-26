#ifndef __MUCLIENT_H__
#define __MUCLIENT_H__

#include<iostream>
#include<stdio.h>
#include<ipv4serverproto.h>
#define __MUDEFAULT_MPLAYER_CMD_PIPE_FILE_PATH__ "/tmp/mplayer_cmd_pipe"
#define __MUDEFAULT_CLIENT_CMD_PIPE_FILE_PATH__ "/tmp/mclient_cmd_pipe"
#define __MUDEFAULT_CLIENT_RET_PIPE_FILE_PATH__ "/tmp/mclient_ret_pipe"
#define __MUDEFAULT_CLIENT_WORK_STATE_OUTPUT_PIPE_FILE_PATH__ "/tmp/mclient_work_state_output_pipe"
#define __MUDEFAULT_PLAYERCMD__ "mplayer - -slave -cache 8192 -cache-min 20 -quiet -input file=/tmp/mplayer_cmd_pipe -ao alsa:device=default > /dev/null"
//将mpg123的文字输出到空设备上

typedef struct ClientConfST
{

	char *rcvport;
	char *mgroup;
	char *player_cmd;
	char *mplayer_cmd_pipe_file_path;
        char *client_cmd_pip_file_path;	
        char *client_ret_pip_file_path;	
	char *client_work_state_output_pipe_file_path;
}client_conf_st;

static client_conf_st client_conf = {\
	.rcvport = DEFAULT_RCVPORT,\
	.mgroup  = DEFAULT_MGROUP,\
	.player_cmd = __MUDEFAULT_PLAYERCMD__,\
	.mplayer_cmd_pipe_file_path = __MUDEFAULT_MPLAYER_CMD_PIPE_FILE_PATH__,\
	.client_cmd_pip_file_path = __MUDEFAULT_CLIENT_CMD_PIPE_FILE_PATH__,
	.client_ret_pip_file_path = __MUDEFAULT_CLIENT_RET_PIPE_FILE_PATH__,
	.client_work_state_output_pipe_file_path = __MUDEFAULT_CLIENT_WORK_STATE_OUTPUT_PIPE_FILE_PATH__

};

static void ClientHelpPrintf()
{
	std::cout
		<< "*-M --mgroup: 指定多播组\n"
 		<< "*-P --port: 指定接收端口\n"
 		<< "*-p --player: 指定解码器\n"
 		<< "*-H --help: 帮助\n"
		<< std::endl;
}

typedef struct MsgChannelDetail
{
	msg_channel_t * data;
	size_t size;
}msg_channel_detail_t;

#endif
