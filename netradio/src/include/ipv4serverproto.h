#ifndef IPV4SERVERPROTO_H
#define IPV4SERVERPROTO_H

#include<concepts>
#include<site_type.h>

#define DEFAULT_MGROUP		"224.5.2.2"
#define DEFAULT_RCVPORT 	"1989"

#define CHNNR			200
#define LISTCHNID		0

#define MINCHNID		1
#define MAXCHNID		(MINCHNID + CHNNR - 1)
#define MSG_CHANNEL_MAX 	(65516 - 20 - 8)
//减去ip包报头和udp包的报头
#define MSG_DATA_MAX		(MSG_CHANNEL_MAX - sizeof(chnid_t))
#define MSG_LIST_MAX		(65516 - 20 - 8)
#define MSG_LIST_ENTRY		(MSG_LIST_MAX - sizeof(chnid_t))

/*template<typename T>
concept ChnData_T = require(T x)
{
	sizeof(T) < MSG_DATA_MAX;
};*/

//template<ChnData_T T>
typedef struct ChannelMsg
{
	chnid_t chnid;
	char data[1];	
}__attribute((packed))msg_channel_t;

typedef struct ChannelListEntry
{
	chnid_t chnid;
	u_int16_t len;
	u_int8_t desc[1];
}__attribute((packed))msg_listentry_t;

typedef struct ChannelList
{
        chnid_t chnid;
        msg_listentry_t entry[1];         
}__attribute((packed))msg_list_t;


#endif
