#include "system.h"

/*ble_port.c*/
//=======================================函数声明========================================

static int8_t ble_proc_ack(void);
static int8_t ble_proc_cmd(void);
static int8_t ble_proc_data(void);

static void ble_send_ack_mess(uint8_t type);
static void ble_send_ctrl_mess(uint16_t reg_addr, uint8_t *data);

static int8_t	ble_prot_retransmission(void);
static int8_t	ble_check_connect_states(void);
static void		ble_end(void);
static uint16_t modbus_crc16(const uint8_t *data, uint16_t length);

//=======================================函数定义========================================

/**
 * @brief 初始化蓝牙协议

 *
 */
void ble_prot_init(void)
{
	ble.last_rx_id = (TRANSACTION_ID << 8) | 0x00;
	ble.tx_id	   = 1;
}

/**
 * @brief 处理透传数据(解包)
 *
 * @return int8_t：返回响应的动作
 */
int8_t ble_proc_depacket(void)
{
	int8_t	crc_ret = 0; // crc校验结果
	uint8_t ret		= ACTION_NONE;
	ble.ack_ret		= ACK_REP_SUCCESS;

	if (((uint16_t)(ble.rx_buf[0] << 8) | ble.rx_buf[1]) == LEAD_CODE) // 校验前导码
	{
		if (ble.connect_states == 0)
			return ACTION_NONE;
		uint16_t len = ble.rx_cnt;
		LOG_D("len:%d", len);
		uint16_t crc = modbus_crc16((uint8_t *)(ble.rx_buf + 2), len - 4);
		LOG_D("crc:0x%04X", crc);

		if (crc != ((uint16_t)(ble.rx_buf[len - 2] << 8) | ble.rx_buf[len - 1])) // 校验CRC
		{
			uint16_t flag = (ble.rx_buf[3] << 8) | ble.rx_buf[4];
			ret			  = ACTION_REQ_LAST_MESS; // 让APP重传
			ble.ack_ret	  = ACK_REP_MESS_CRC_ERR;
			ble_send_ack_mess(ble.ack_ret);
			rt_thread_delay(10);
			return ret;
		}
		ble.first_pack	 = (ble.rx_buf[3] & 0x80); // 首包标志位
		uint8_t versions = ble.rx_buf[2];
		// 区分版本(不同版本动作不同)
		if (versions == VERSION)
		{
			uint16_t flag = (ble.rx_buf[3] << 8) | ble.rx_buf[4];  //根据定的协议来解析
			switch (flag & 0x0003) 
			{
				// 应答消息
				case 1:
					ret = ble_proc_ack();
					return ret;

				// 命令消息
				case 2:
					ret = ble_proc_cmd();
					return ret;

				// 数据消息
				case 3:
					ret = ble_proc_data();
					return ret;

				default:
					ble.ack_ret = ACK_REP_MESS_TYPE_ERR;
					ret			= ACTION_REQ_LAST_MESS;
					break;
			}
		}
		else
		{
			ret = ACTION_REQ_LAST_MESS;
		}
	}
	else
	{
		if (ble_check_connect_states() != 0)
		{
			ret = ACTION_NONE;
		}
	}
	return ret;
}

/**
 * @brief 处理透传数据(响应)
 *
 * @param type：透传解包后得到的类型
 */
void ble_response(int8_t type)
{
	ble_rx_reset(ble.rx_cnt);
	uint8_t req_data[3] = {0};
	switch (type)
	{
		// 请求上一消息
		case ACTION_REQ_LAST_MESS:
			req_data[0] = 2;
			req_data[1] = ble.req_data >> 8;
			req_data[2] = ble.req_data & 0xFF;
			ble_send_ctrl_mess(0x000F, req_data);
			break;

		// 重传上一次的发送
		case ACTION_RE_SEND:
			ble_prot_retransmission(); // 重传上一次的发送
			break;

		default:
			break;
	}
}

//=======================================蓝牙处理应答、控制、数据消息========================================

/**
 * @brief  	处理应答消息(根据不同的应答消息做不同的动作)
 * 			1. 判断事务id是否正确
 * 			2. 判断是否是应答寄存器
 * 			3. 判断是否存在错误应答回应
 *
 * @return int8_t
 */
static int8_t ble_proc_ack(void)
{
	int8_t	 action_ret	  = ACTION_NONE;
	uint16_t rx_id		  = (ble.rx_buf[5] << 8) | ble.rx_buf[6];
	uint16_t ack_reg_addr = (ble.rx_buf[9] << 8) | ble.rx_buf[10];
	uint16_t ack_info_len = (ble.rx_buf[11] << 8) | ble.rx_buf[12];
	uint16_t id			  = (TRANSACTION_ID << 8) | ble.tx_id;

	if ((rx_id != id) || (ack_reg_addr != 0x0000) || (ack_info_len != 0x01)) // 应答消息本身有误，让APP重传应答消息
	{
		ble.req_data = 0xFFFF;
		action_ret	 = ACTION_REQ_LAST_MESS;
		goto over;
	}

	switch (ble.rx_buf[13])
	{
		case ACK_REP_SUCCESS:
			break;

		// 事务id错误，等待APP要求重传的事务id消息，所以无动作
		case ACK_REP_ID_ERR:
			break;

		// 其他错误，都是重传上一消息
		case ACK_REP_MESS_TYPE_ERR:
		case ACK_REP_REG_ERR:
		case ACK_REP_MESS_LEN_ERR:
		case ACK_REP_MESS_DATA_ERR:
		case ACK_REP_MESS_CRC_ERR:
			action_ret = ACTION_RE_SEND;
			break;

		// 应答消息错误，让APP重传应答消息
		default:
			ble.req_data = 0xFFFF;
			action_ret	 = ACTION_REQ_LAST_MESS;
			break;
	}
over:
	return action_ret;
}

/**
 * @brief 	处理命令消息
 * 			1. 判断事务id是否正确(较前 + 1)
 * 			2. 判断命令寄存器是否有效
 *
 * @return int8_t
 */
static int8_t ble_proc_cmd(void)
{
	int8_t	 action_ret	   = ACTION_NONE;
	uint16_t rx_id		   = (ble.rx_buf[5] << 8) | ble.rx_buf[6];
	uint16_t cmd_reg_addr  = (ble.rx_buf[9] << 8) | ble.rx_buf[10];
	uint8_t *payload_start = (uint8_t *)(ble.rx_buf + 9);
	ble.ack_ret			   = ACK_REP_SUCCESS;

	// if (rx_id != (ble.last_rx_id + 1))
	// {
	// 	ble.ack_ret	 = ACK_REP_ID_ERR;
	// 	ble.req_data = (ble.last_rx_id + 1); // 重新请求的是目标事务id的消息
	// 	action_ret	 = ACTION_REQ_LAST_MESS;
	// 	goto ack;
	// }

	if ((cmd_reg_addr & 0x7FFF) > CMD_ACTION_MAX_NUMS) // 命令寄存器地址大于最大值
	{
		ble.req_data = rx_id; // 重新请求的是当前消息
		ble.ack_ret	 = ACK_REP_REG_ERR;
		action_ret	 = ACTION_REQ_LAST_MESS;
		goto ack;
	}

ack:
	ble_send_ack_mess(ble.ack_ret); // 响应ack，然后再根据不同的指令返回响应动作
	rt_thread_delay(10);			// 等待ack发送完成

	if (ble.ack_ret != ACK_REP_SUCCESS)
	{
		return action_ret; // 如果ack不是成功，则直接返回
	}
	ble.last_rx_id = rx_id;

	// 根据命令处理动作
	if (cmd_reg_addr & 0x8000)
	{
		// 产品自定义命令消息
		ble_proc_custom_cmd(payload_start);
	}
	else
	{
		// 通用的指令
		ble_proc_general_cmd(payload_start);
	}
	return action_ret;
}

/**
 * @brief 处理数据消息
 * 			1. 判断事务id是否正确(较前 + 1)
 * 			2. 判断数据寄存器是否有效
 *
 * @return int8_t
 */
static int8_t ble_proc_data(void)
{
	int8_t	 action_ret	   = ACTION_NONE;
	uint16_t rx_id		   = (ble.rx_buf[5] << 8) | ble.rx_buf[6];
	uint16_t data_reg_addr = (ble.rx_buf[9] << 8) | ble.rx_buf[10];
	uint8_t *payload_start = (uint8_t *)(ble.rx_buf + 9);
	ble.ack_ret			   = ACK_REP_SUCCESS;

	// if (rx_id != (ble.last_rx_id + 1))
	// {
	// 	ble.ack_ret	 = ACK_REP_ID_ERR;
	// 	ble.req_data = (ble.last_rx_id + 1); // 重新请求的是目标事务id的消息
	// 	action_ret	 = ACTION_REQ_LAST_MESS;
	// 	goto ack;
	// }

	if ((data_reg_addr & 0x7FFF) > DATA_ACTION_MAX_NUMS) // 命令寄存器地址大于最大值
	{
		ble.req_data = rx_id; // 重新请求的是当前消息
		ble.ack_ret	 = ACK_REP_REG_ERR;
		action_ret	 = ACTION_REQ_LAST_MESS;
		goto ack;
	}

ack:
	ble_send_ack_mess(ble.ack_ret); // 响应ack，然后再根据不同的指令返回响应动作
	rt_thread_delay(10);			// 等待ack发送完成

	if (ble.ack_ret != ACK_REP_SUCCESS)
	{
		return action_ret; // 如果ack不是成功，则直接返回
	}
	ble.last_rx_id = rx_id;

	// 根据数据处理动作
	if (data_reg_addr & 0x8000)
	{
		// 产品自定义命令消息
		ble_proc_custom_data(payload_start);
	}
	else
	{
		// 通用的指令
		ble_proc_general_data(payload_start);
	}
	return action_ret;
}

//=======================================蓝牙发送数据========================================

/**
 * @brief 发送应答消息
 *
 */
static void ble_send_ack_mess(uint8_t type)
{
	ble.tx_buf[0] = LEAD_CODE >> 8;
	ble.tx_buf[1] = LEAD_CODE & 0xFF;
	ble.tx_buf[2] = VERSION;
	ble.tx_buf[3] = 0xA0; // 消息标志
	ble.tx_buf[4] = 0x01;
	ble.tx_buf[5] = ble.last_rx_id >> 8; // 接收到的事务id
	ble.tx_buf[6] = ble.last_rx_id & 0xFF;
	ble.tx_buf[7] = 0x00;
	ble.tx_buf[8] = 0x03;

	ble.tx_buf[9]  = 0x00;
	ble.tx_buf[10] = 0x00;

	ble.tx_buf[11] = type;

	// 计算CRC
	uint16_t crc   = modbus_crc16((uint8_t *)(ble.tx_buf + 2), 10);
	ble.tx_buf[12] = crc >> 8;
	ble.tx_buf[13] = crc & 0xFF;

	ble_send_data(14);
}

/**
 * @brief 发送控制消息
 *
 * @param reg_addr：寄存器地址
 * @param data：数据指针(首字节长度)
 */
static void ble_send_ctrl_mess(uint16_t reg_addr, uint8_t *data)
{
	uint16_t tx_len	  = 0;
	uint16_t data_len = 0;

	ble.tx_buf[0]  = LEAD_CODE >> 8;
	ble.tx_buf[1]  = LEAD_CODE & 0xFF;
	ble.tx_buf[2]  = VERSION;
	ble.tx_buf[3]  = 0x80; // 消息标志
	ble.tx_buf[4]  = 0x02;
	ble.tx_buf[5]  = TRANSACTION_ID; // 发送事务id
	ble.tx_buf[6]  = ble.tx_id;
	ble.tx_buf[7]  = 0x00;
	ble.tx_buf[8]  = 0x05;
	ble.tx_buf[9]  = reg_addr >> 8;
	ble.tx_buf[10] = reg_addr & 0xFF;

	tx_len = 11 + 2;

	if (data != NULL)
	{
		data_len = (*data++); // 获取数据长度
		tx_len += data_len;
		for (uint8_t i = 0; i < data_len; i++)
		{
			ble.tx_buf[13 + i] = (*data++); // 数据填充
		}
	}
	ble.tx_buf[11] = data_len >> 8;
	ble.tx_buf[12] = data_len & 0xFF;

	// 计算CRC
	uint16_t crc		   = modbus_crc16((uint8_t *)(ble.tx_buf + 2), tx_len - 2);
	ble.tx_buf[tx_len]	   = crc >> 8;
	ble.tx_buf[tx_len + 1] = crc & 0xFF;
	ble.tx_id			   = (ble.tx_id == 0xFF) ? 0x01 : (ble.tx_id + 1); // 事务id

	ble_send_data(tx_len + 2);
}

/**
 * @brief 发送数据消息
 *
 * @param reg_addr：寄存器地址
 * @param data：数据指针(首字节长度)
 */
void ble_send_data_mess(uint16_t reg_addr, uint8_t *data)
{
	uint16_t payload_len = 0;
	uint16_t data_len	 = 0;

	ble.tx_buf[0] = LEAD_CODE >> 8;
	ble.tx_buf[1] = LEAD_CODE & 0xFF;
	ble.tx_buf[2] = VERSION;
	ble.tx_buf[3] = 0xA0; // 消息标志
	ble.tx_buf[4] = 0x03;
	ble.tx_buf[5] = 0x00; // 发送事务id
	ble.tx_buf[6] = ble.tx_id;

	data_len = 9; // 消息头长度

	if (data != NULL)
	{
		payload_len = (*data++); // 获取有效载荷长度
		data_len += payload_len;
		for (uint8_t i = 0; i < payload_len; i++)
		{
			ble.tx_buf[9 + i] = (*data++); // 数据填充
		}
	}

	// 有效载荷
	ble.tx_buf[7] = payload_len >> 8;
	ble.tx_buf[8] = payload_len & 0xFF;

	// 计算CRC
	uint16_t crc			 = modbus_crc16((uint8_t *)(ble.tx_buf + 2), data_len - 2);
	ble.tx_buf[data_len]	 = crc >> 8;
	ble.tx_buf[data_len + 1] = crc & 0xFF;
	ble.tx_id				 = (ble.tx_id == 0xFF) ? 0x01 : (ble.tx_id + 1); // 事务id
	ble_send_data(data_len + 2);
}

//=======================================其他函数========================================

/**
 * @brief 填充+启动
 *
 * @param cmd
 */
void ble_send_data(uint16_t cnt)
{
	if (rt_sem_take(ble.tx_sem, RT_WAITING_FOREVER) == RT_EOK)
	{
		ble.last_tx_len = cnt;
		USART_Enable(BLE_TX_UART, DISABLE);
		DMA_EnableChannel(BLE_UART_TX_DMA_CH, DISABLE);
		BLE_UART_TX_DMA_CH->TXNUM = cnt;
		BLE_UART_TX_DMA_CH->MADDR = (uint32_t)ble.tx_buf;
		DMA_EnableChannel(BLE_UART_TX_DMA_CH, ENABLE);
		USART_Enable(BLE_TX_UART, ENABLE);
	}
}

/**
 * @brief  重发协议数据
 *
 * @return int8_t
 */
static int8_t ble_prot_retransmission(void)
{
	if (ble.retransmit_cnt < 3)
	{
		ble.retransmit_cnt++;
		// 处理发送
		ble_send_data(ble.last_tx_len);
		return 0;
	}
	else
	{
		ble_end(); // 结束本次通讯
		return -1;
	}
}

/**
 * @brief 检查蓝牙连接状态
 *
 */
static int8_t ble_check_connect_states(void)
{
	// 这里检测下是不是断开连接了
	for (uint16_t i = 0; i < ble.rx_cnt; i++)
	{
		if (ble.rx_buf[i] == 'T')
		{
			if (memcmp(&ble.rx_buf[i], "TTM:DISCONNECT", 14) == 0)
			{
				LOG_D("ble disconnected");
				ble_end(); // 结束本次通讯
				ble.connect_states = 0;
				return 0;
			}
			else if (memcmp(&ble.rx_buf[i], "TTM:CONNECTED", 13) == 0)
			{
				LOG_D("ble connected");
				ble.connect_states = 1; // 设置连接状态
				return 0;
			}
		}
	}
	return -1;
}

/**
 * @brief 蓝牙结束本次通讯
 *
 */
static void ble_end(void)
{
	ble.retransmit_cnt = 0;							   // 重发次数清除
	ble.rx_pack_len	   = 0;							   // 接收包长度清除
	ble.rx_cnt		   = 0;							   // 接收数据长度
	ble.tx_cnt		   = 0;							   // 发送数据长度
	ble.last_tx_len	   = 0;							   // 上一次发送数据长度(重发用到)
	ble.tx_id		   = 0x01;						   // 发送的事务id
	ble.last_rx_id	   = (TRANSACTION_ID << 8) | 0x00; // 上一次接收的事务id
	ble.ack_ret		   = ACK_REP_SUCCESS;			   // ack返回值
	ble.req_data	   = 0;							   // 请求的数据
}

/**
 * @brief 清除蓝牙接收缓冲区
 *
 * @param parameter
 */
static void ble_clear_rx_buf(void *parameter)
{
	memset(ble.rx_buf, 0, BLE_RX_MAX_LEN); // 清空接收缓冲区
	ble.rx_cnt		= 0;				   // 重置接收计数
	ble.rx_pack_len = 0;				   // 接收包长度清除
}

/**
 * @brief 计算modbus crc16校验值

 *
 * @param data: 数据指针
 * @param length: 数据长度
 * @return uint16_t：大端模式
 */
static uint16_t modbus_crc16(const uint8_t *data, uint16_t length)
{
	if (length > BLE_RX_MAX_LEN)
		return 0; // 防止越界

	uint16_t crc = 0xFFFF; // 初始值
	for (uint16_t i = 0; i < length; i++)
	{
		crc ^= (uint16_t)data[i]; // 异或当前字节
		for (uint8_t j = 0; j < 8; j++)
		{
			if (crc & 0x0001)
			{				   // 检查最低位
				crc >>= 1;	   // 右移一位
				crc ^= 0xA001; // 多项式0x8005的反转形式（0xA001）
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return crc;
}
