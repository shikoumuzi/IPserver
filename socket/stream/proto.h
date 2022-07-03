
#ifndef SOCPROTO
#define SOCPROTO


#define RCVPOT "1989"
//确定被动端所绑定的端口号 同时利用字符串的形式，确定数值的单位
#define MAXSIZE 1024



typedef struct MSGST
{
	u_int8_t name[MAXSIZE];
	u_int32_t math;
	u_int32_t chinese;

}__attribute__((packed)) msg_st;



#endif
