#ifndef __BLE_H__
#define __BLE_H__
// clang-format off

#define 	CMD_ACTION_MAX_NUMS		30
#define 	DATA_ACTION_MAX_NUMS	30



// 在 ble.h 中添加
#define ACK_TIMEOUT_MS         50    // ACK超时时间(毫秒)
#define MAX_RETRANSMIT_COUNT    3       // 最大重传次数
#define ACK_CHECK_INTERVAL_MS   10     // ACK检查间隔(毫秒)
// 新增事件
#define EVENT_ACK_TIMEOUT       0x10    // ACK超时事件






// 事件
#define 	EVENT_RECV			0x01
#define 	EVENT_BLE_CON		0x04
//预留的两个事件
#define 	EVENT_BLE_DIS		0x08
#define 	EVENT_PROT_SEND		0x02
//所有事件
#define 	ALL_EVENT			(EVENT_RECV | EVENT_PROT_SEND | EVENT_BLE_CON | EVENT_BLE_DIS)


//前导码
#define 	LEAD_CODE			0xAA55
//版本
#define 	VERSION				0x12

//自定义命令
typedef enum
{
	CUSTOM_CMD	=1,
	GENERAL_CMD	,
	CUSTOM_DATA	,
	GENERAL_DATA,
}TYPE_t;



//=================这个ble头文件只定义线程相关的以及数据变量======================

/*回调函数类型*/
typedef void (*ACTION_CB)(uint8_t *);
/*动作结构体*/
typedef struct
{
	uint16_t reg;
	ACTION_CB cb;
}ACTION_t;


// 蓝牙线程数据结构
typedef struct
{
	rt_thread_t thread;   // 蓝牙线程
	rt_event_t  event;	// 接收事件
	rt_sem_t    tx_sem;	// 同步发送完
	rt_timer_t 	timer;	// 重置接收缓冲区定时器--1次性
	rt_timer_t 	check_ack_timer;	// 检查ACK超时定时器
}BLE_THREAD_t;
extern BLE_THREAD_t ble_thread;

// 蓝牙数据结构
typedef struct
{
	char 	*rx_buf;   // 接收缓冲区
	char 	*tx_buf;   // 发送缓冲区
	char at_rx_param[30];	// AT指令接收参数

	uint8_t bind_flag;	// 绑定标志位，0:未绑定，1:已绑定

	uint8_t connect_states;	// 蓝牙连接状态，0:未连接，1:已连接
	uint8_t retransmit_cnt;	// 重发次数


	uint32_t ack_timeout_start;  // ACK超时开始时间


	// 透传数据协议
	uint16_t rx_pack_len;	// 接收包长度--接收后清0
	uint16_t rx_cnt;		// 接收数据长度

	uint16_t tx_cnt;		// 发送数据长度
	uint16_t last_tx_len;	// 上一次发送数据长度(重发用到)


	uint16_t  tx_id;			// 发送的事务id
	uint16_t last_rx_id;	// 上一次接收的事务id

	uint8_t  ack_ret;		// ack返回值
	uint16_t req_data;		// 请求的数据


	uint32_t firmware_len;	// 固件升级包最大长度


	SENT_t send_type;	// 发送类型-每次mcu发送需要记录发送的类型


	// 自定义命令处理动作
	ACTION_t custom_cmd_action[CMD_ACTION_MAX_NUMS];
	uint8_t  custom_cmd_cnt;
	// 通用命令处理动作
	ACTION_t general_cmd_action[CMD_ACTION_MAX_NUMS];
	uint8_t  general_cmd_cnt;
	// 自定义数据处理动作
	ACTION_t custom_data_action[DATA_ACTION_MAX_NUMS];
	uint8_t  custom_data_cnt;
	// 通用数据处理动作
	ACTION_t general_data_action[DATA_ACTION_MAX_NUMS];
	uint8_t  general_data_cnt;
}BLE_t;
extern BLE_t ble;


// 系统的关于信息
typedef struct
{
	char model[10];		// 产品型号
	char sn[10];		// 产品序列号
	uint16_t hardware;	// 硬件版本号
	uint32_t software;	// 软件版本号
	uint8_t type;		// 产品大类
	uint8_t subtype;	// 产品次类
}ABOUT_t;











//===========================================
void ble_rx_isr(void);
void ble_tx_isr(void);
void ble_init(void);
uint8_t ble_reg_addr_register(uint8_t type, uint16_t reg, ACTION_CB cb);

int8_t ble_proc_custom_cmd(uint8_t *payload);
int8_t ble_proc_general_cmd(uint8_t *payload);
int8_t ble_proc_custom_data(uint8_t *payload);
int8_t ble_proc_general_data(uint8_t *payload);

void ble_rx_reset(uint16_t last_rx_len);


//===========================================


#endif // __BLE_H__