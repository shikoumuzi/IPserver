#ifndef MPROTO
#define MPROTO
#include<concepts>

#define PATHSIZE 	100
#define MPATHNAME 	"../msg"
#define MPROJ		'M'

template<typename T>
concept __data_type = requires(T x)
{
	sizeof(T) < 128;
};

enum
{
	MMSGPATH = 0X00000001L,
	MMSGDATA,
	MMSGEND
};

typedef struct MsgPath
{
	long mtype;
	char pathname[PATHSIZE];
}__attribute__((packed)) msg_path_t;

template<__data_type T>
struct MsgData
{
	long mtype; 
        T data;
}__attribute__((packed));

template<__data_type T>
using msg_data_t = struct MsgData<T>; 

typedef struct MsgEnd
{
	long mtype;
	const char endstr[PATHSIZE];

}__attribute__((packed)) msg_end_t;

template<typename T>
union DataPackage
{
	long mtype;
	//这里采用联合体来囊括数据包，
	//由于联合体的特性，且我们所定义的结构体前四个字节均为long mtype
	//所以当要判定是哪个数据包时，只需要根据mtype即可判定数据包类型
	msg_path_t msgpath;
	msg_data_t<T> msgdata;
	msg_end_t msgend;
};


#endif

//此为消息队列数据的原型
// 其中 mtype是用来对数据包进行标记，以区分包的种类
// 而 mtext[1] 代表这是一个变长的结构体， 可以存放任意的数据结构
//struct msgbuf {
//	long mtype;       // message type, must be > 0 
//	char mtext[1];    // message data //
//};
//
