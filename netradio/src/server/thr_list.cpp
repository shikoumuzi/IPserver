

#include<iostream>
#include<unistd.h>
#include<pthread.h>
#include<sys/types.h>
#include<sys/socket.h>
#include"server_conf.h"


int thr_list_create(mlib_listentry_t *mliblist , int size);
int thr_list_destory();

