
#ifndef SOCPROTO
#define SOCPROTO


#define RCVPOT "1989"
//确定被动端所绑定的端口号 同时利用字符串的形式，确定数值的单位
#define MAXSIZE 512-8-8
//512是udp报的推荐长度，8是int32的字节数



typedef struct MSGST
{
	u_int32_t math;
	u_int32_t chinese;
	u_int8_t name[1];//动态结构体的内容
}__attribute__((packed)) msg_st;



#endif
