#ifndef __BLE_PROT_H__
#define __BLE_PROT_H__
// clang-format off

#define 	TRANSACTION_ID 		0x00



//===========================================
// ack上报的类型--检查接收的数据，然后发送给APP的ack信号
typedef enum
{
	ACK_REP_SUCCESS 		= 0x01,		// 上报应答ok
	ACK_REP_ID_ERR 			= 0x02,		// 事务id错误
	ACK_REP_MESS_TYPE_ERR 	= 0x03,		// 上报消息类型错误
	ACK_REP_REG_ERR 		= 0x04,		// 上报寄存器错误
	ACK_REP_MESS_LEN_ERR 	= 0x05,		// 上报消息长度错误
	ACK_REP_MESS_DATA_ERR 	= 0x06,		// 上报消息数据错误
	ACK_REP_MESS_CRC_ERR 	= 0x07,		// 上报消息crc错误
}ACK_REP_TYPE;


// 解包后的“动作”
typedef enum
{
	ACTION_NONE = 0,		// 无动作
	ACTION_REQ_LAST_MESS,	// 请求最后一包数据----MCU -> APP
	ACTION_RE_SEND,			// 重新发送-----------APP -> MCU
}DEPACKET_ACTION_t;



// 发送的类型定义
typedef enum
{
	SEND_NONE = 0,		// 缺省值

	MCU_SEND_CMD,
	MCU_SEND_DATA,	
	MCU_SEND_ACK,	


}SENT_t;




//===========================================

void ble_prot_init(void);
int8_t ble_proc_depacket(void);
void ble_response(int8_t type);

void ble_send_data_mess(uint16_t reg_addr, uint8_t *data);
void ble_send_data(uint16_t cnt);

void ble_clear_rx_buf(void *parameter);
void ble_end(void);
void ble_clear_ack_timer(void);
int8_t ble_prot_retransmission(void);
//===========================================

#endif // __BLE_PROT_H__