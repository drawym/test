#ifndef __BLE_AT_H__
#define __BLE_AT_H__
// clang-format off


/*测试接口--打开可执行测试代码，判断串口是否正常工作*/
#define Ble_AT_DEBUG 0
#if defined(Ble_AT_DEBUG)
// #include "SEGGER_RTT.h"
#endif

//定义打印的开关
#define Ble_AT_DEBUG_PRINT 1
#if Ble_AT_DEBUG_PRINT
//定义调试打印函数
#include "SEGGER_RTT.h"
#define Ble_AT_PRINTF(...) LOG_D(__VA_ARGS__)
#else
#define Ble_AT_PRINTF(...)
#endif




//AT指令重试次数
#define Ble_AT_Retries 3



/*定义蓝牙AT指令返回符--返回带有下面的符合视为AT指令成功*/
#define Ble_AT_Return_Key "+"


/*
    定义本蓝牙获取mac地址 的 AT指令 的 关键标识符--可能有点绕，简单来说，就是
    发送了获取mac的命令后，蓝牙模块会返回数据，但是数据中包含很多其他信息，我们需要找到mac地址的前的一些"标志字符"
    比如获取mac的命令后，蓝牙模块会返回"0x112233445566"，我们希望的mac地址是"112233445566",
    则Ble_AT_Mac_Key是"0x"
*/
#define Ble_AT_Mac_Key   "+LADDR="
#define Ble_AT_Mac_len   6   //mac的字节数 98dab0107941 则是6个字节
#define Ble_AT_Mac_Key_Len (sizeof(Ble_AT_Mac_Key) - 1)  /*无需修改，Ble_AT_Mac_Key的长度，用来计算mac地址的偏移量*/



//===========================================
// BLE UUID 配置宏定义
//===========================================


//===========================================
// 定义参数类型枚举
typedef enum {
    ARG_TYPE_NONE,    	// 无参数
    ARG_TYPE_STRING,  	// 字符串类型
    ARG_TYPE_INT,     	// 整数类型
    ARG_TYPE_HEX_ARRAY,	// 十六进制数组类型
} ARG_TYPE_t;
//===========================================

void ble_at_config(void);
int8_t ble_at_send(char *cmd, rt_int32_t timeout);

//===========================================

#endif // __BLE_AT_H__