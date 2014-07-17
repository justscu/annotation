#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum TYPE_OPER
{
	TYPE_OPER_Subscribe = 1, // 客户端订阅
	TYPE_OPER_UnSubscribe = 2, // 客户取消订阅
	TYPE_OPER_DelChannel = 3, // 删除通道
};

// i'll help you call df_init_zmq() and df_destroy_mq() in main()
unsigned int df_init_zmq();
unsigned int df_destroy_mq();

void df_sensus_subscribers(const char* str, unsigned int strLen, unsigned int subers, enum TYPE_OPER ope);


