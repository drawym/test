#include "system.h"
#include <string.h>
#include <stdio.h>

ABOUT_t about;
BLE_t ble;
BLE_THREAD_t ble_thread;
/*ble.c*/
//=======================================函数声明========================================
static void ble_thread_func(void *parameter);
static void ble_read_info(uint8_t *arg);
static void ble_verifies_info(uint8_t *arg);
static void ble_ota_get_firmware_len(uint8_t *arg);
static void ble_ota_firmware(uint8_t *arg);
void ble_ack_timeout(void);
static void ble_retransmit_info(uint8_t *arg);
//=======================================函数定义========================================

//=======================================中断处理函数========================================
/**
 * @brief 蓝牙接收中断服务函数(串口空闲中断)
 */
void ble_rx_isr(void)
{
	LOG_D("rx over");
	/*按照协议判断，正确的话发送接收事件，去任务处理*/
	if (ble.rx_buf[0] == 0xAA && ble.rx_buf[1] == 0x55) // 前导码
	{
		LOG_D("rx over packet start");
		rt_tick_t tick = 1000;
		// if (ble.rx_pack_len == 0) 
		// {
		// 	rt_timer_control(ble_thread.timer, RT_TIMER_CTRL_SET_TIME, &tick); // 重新定时器
		// 	rt_timer_start(ble_thread.timer);								   // 启动定时器
		// }
		/*计算接收包长度*/
		ble.rx_pack_len = ((ble.rx_buf[7] << 8) | ble.rx_buf[8]) + 11; // 数据包中的数据长度
		ble.rx_cnt = BLE_RX_MAX_LEN - BLE_UART_RX_DMA_CH->TXNUM;	   // 实际接收到的数据长度
		LOG_D("rx over packet len: %d", ble.rx_pack_len);
		LOG_D("rx over cnt: %d", ble.rx_cnt);
		/*判断接收包长度是否正确*/
		if (ble.rx_pack_len == ble.rx_cnt)
		{
			LOG_D("rx over packet end");
			ble.rx_pack_len = 0;
			rt_event_send(ble_thread.event, EVENT_RECV); // 发送接收事件
			// rt_timer_stop(ble_thread.timer);			 // 停止定时器
		}
		else  //错误的情况
		{
			LOG_D("ble.rx_pack_len != ble.rx_cnt");
			ble_rx_reset(ble.rx_cnt);      //重置接收缓冲区
		}
	}
	else // 不是前导码，可能是AT指令等
	{
		LOG_D("rx over packet error no 0xAA 0x55");
		ble.rx_cnt = BLE_RX_MAX_LEN - BLE_UART_RX_DMA_CH->TXNUM;
		rt_event_send(ble_thread.event, EVENT_RECV); // 发送接收事件

		// // //将接收到的数据打印出来
		// LOG_D("!!!!!!!!========  rx data  ========!!!!!!!!    %s", ble.rx_buf);
	}
}
/**
 * @brief 蓝牙发送中断服务函数(DMA完成中断)
 */
void ble_tx_isr(void)
{
	LOG_D("tx over");
	rt_sem_release(ble_thread.tx_sem); // 发送完成事件
}
//=======================================中断处理函数========================================






//=======================================主任务========================================
//=======================================主任务========================================
//=======================================主任务========================================
//=======================================主任务========================================

/**
 * @brief 蓝牙初始化
 *
 */
void ble_init(void)
{
	bsp_ble_init();							   /*底层初始化  GPIO，中断，DMA*/
	bsp_ble_set_buf(&ble.rx_buf, &ble.tx_buf); /*指向接收和发送缓冲区*/
	bsp_ble_enable_uart(1);					   /*打开串口*/

	////注册命令消息的动作--这个是接收到APP发来的命令/数据之后，处理的动作
	ble_reg_addr_register(GENERAL_CMD, 0x0001, ble_read_info); // 回调函数的形参是有效载荷的首地址
	ble_reg_addr_register(GENERAL_CMD, 0x0002, ble_read_info);
	ble_reg_addr_register(GENERAL_CMD, 0x0003, ble_read_info);
	ble_reg_addr_register(GENERAL_CMD, 0x0004, ble_read_info);
	ble_reg_addr_register(GENERAL_CMD, 0x0005, ble_read_info);
	ble_reg_addr_register(GENERAL_CMD, 0x0006, ble_read_info);
	ble_reg_addr_register(GENERAL_CMD, 0x0007, ble_read_info);
	ble_reg_addr_register(GENERAL_CMD, 0x0008, ble_read_info);
	ble_reg_addr_register(GENERAL_CMD, 0x0010, ble_verifies_info); // 实现RSA加密


	/*CMD-错误重传上一帧数据*/
	ble_reg_addr_register(GENERAL_CMD, 0x000F, ble_retransmit_info);

	// 这里，APP先发送固件升级的命令，然后APP再发送固件升级的数据
	ble_reg_addr_register(GENERAL_CMD, 0x000E, ble_ota_get_firmware_len); // 获取固件长度
	ble_reg_addr_register(GENERAL_DATA, 0x0009, ble_ota_firmware);		  // 固件升级

	////关于
	about.hardware = 0x0101;	  // 硬件版本1.1
	about.software = 0x00001;	  // 软件版本0.0.1
	strcpy(about.model, "HRM20"); // 型号
	strcpy(about.sn, "SN000001"); // 序列号
	about.type = 0x01;			  // 大类
	about.subtype = 0x01;		  // 次类

	ble_thread.thread = rt_thread_create("ble_thread", ble_thread_func, RT_NULL, 1024, BLE_THREAD_PRIORITY, 20);
	ble_thread.event = rt_event_create("ble_event", RT_IPC_FLAG_FIFO);										 // 事件--串口空闲中断用来通知蓝牙线程有事件发生
	ble_thread.tx_sem = rt_sem_create("ble_tx_sem", 1, RT_IPC_FLAG_FIFO);									 // 信号量--DMA传输完成中断用来同步等待发送完成
	ble_thread.timer = rt_timer_create("ble_timer", ble_clear_rx_buf, RT_NULL, 100, RT_TIMER_FLAG_ONE_SHOT); // 定时器--串口空闲中断用来重置接收缓冲区
	ble_thread.check_ack_timer = rt_timer_create("ble_timer", ble_ack_timeout, RT_NULL, rt_tick_from_millisecond(ACK_CHECK_INTERVAL_MS),  RT_TIMER_FLAG_PERIODIC); // 定时器--检查ACK超时


	rt_thread_delay(1000);
	rt_thread_startup(ble_thread.thread);
}




/*100ms检测一次ack超时*/
void ble_ack_timeout(void)
{

    uint32_t current_time = rt_tick_get();
    if ((current_time - ble.ack_timeout_start) >= rt_tick_from_millisecond(ACK_TIMEOUT_MS))
    {
        LOG_D("ACK timeout");
        ble.retransmit_cnt++;
        
        if (ble.retransmit_cnt >= MAX_RETRANSMIT_COUNT)
        {
            LOG_D("Max retransmit reached, ending communication");
			/*通知上层应用，蓝牙连接超时*/
            ble_end();
            return;
        }
        
        // 重传
        ble.retransmit_cnt++;
        ble_send_data(ble.last_tx_len);

    }
    
}




/**
 * @brief 蓝牙线程函数
 * @param parameter
 */
static void ble_thread_func(void *parameter)
{
	LOG_D("ble_thread ok");
	rt_uint32_t recved = 0;

	ble_at_config(); // 配置蓝牙AT指令，配置数据包，连接等待，完成连接跳出

	while (1)
	{
		/*接收事件*/
		if (rt_event_recv(ble_thread.event, ALL_EVENT, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, &recved) == RT_EOK)
		{
			/*接收事件*/
			if ((recved & EVENT_RECV) == EVENT_RECV)
			{
				LOG_D("recved: %d", recved);
				ble_response(ble_proc_depacket()); // 处理透传数据
			}
		}

	}
}

/**
 * @brief 寄存器地址注册
 *
 * @param reg
 * @param data
 * @param len
 * @return int8_t
 */
uint8_t ble_reg_addr_register(TYPE_t type, uint16_t reg, ACTION_CB cb)
{
	switch (type)
	{
	case CUSTOM_CMD:
		ble.custom_cmd_action[ble.custom_cmd_cnt].reg = reg;
		ble.custom_cmd_action[ble.custom_cmd_cnt].cb = cb;
		ble.custom_cmd_cnt++;
		return ble.custom_cmd_cnt;

	case GENERAL_CMD:
		ble.general_cmd_action[ble.general_cmd_cnt].reg = reg;
		ble.general_cmd_action[ble.general_cmd_cnt].cb = cb;
		ble.general_cmd_cnt++;
		return ble.general_cmd_cnt;

	case CUSTOM_DATA:
		ble.custom_data_action[ble.custom_data_cnt].reg = reg;
		ble.custom_data_action[ble.custom_data_cnt].cb = cb;
		ble.custom_data_cnt++;
		return ble.custom_data_cnt;

	case GENERAL_DATA:
		ble.general_data_action[ble.general_data_cnt].reg = reg;
		ble.general_data_action[ble.general_data_cnt].cb = cb;
		ble.general_data_cnt++;
		return ble.general_data_cnt;

	default:
		return 0;
	}
}

/**
 * @brief 处理自定义命令
 *
 * @return int8_t
 */
int8_t ble_proc_custom_cmd(uint8_t *payload)
{
	uint16_t reg = (payload[0] << 8) | payload[1]; // 有效载荷
	for (uint8_t i = 0; i < ble.custom_cmd_cnt; i++)
	{
		if ((reg == ble.custom_cmd_action[i].reg) && (ble.custom_cmd_action[i].cb != NULL))
		{
			// 执行命令
			ble.custom_cmd_action[i].cb(payload);
		}
	}
	return 0;
}

/**
 * @brief 处理通用的命令
 *
 * @param reg
 * @return int8_t
 */
int8_t ble_proc_general_cmd(uint8_t *payload)
{
	uint16_t reg = (payload[0] << 8) | payload[1]; // 取出地址，根据定义地址是2字节
	for (uint8_t i = 0; i < ble.general_cmd_cnt; i++)
	{
		if ((reg == ble.general_cmd_action[i].reg) && (ble.general_cmd_action[i].cb != NULL))
		{
			// 执行命令
			ble.general_cmd_action[i].cb(payload);
		}
	}
	return 0;
}

/**
 * @brief 处理自定义数据
 *
 * @param reg
 * @return int8_t
 */
int8_t ble_proc_custom_data(uint8_t *payload)
{
	uint16_t reg = (payload[0] << 8) | payload[1];
	for (uint8_t i = 0; i < ble.custom_data_cnt; i++)
	{
		if ((reg == ble.custom_data_action[i].reg) && (ble.custom_data_action[i].cb != NULL))
		{
			// 执行命令
			ble.custom_cmd_action[i].cb(payload);
		}
	}
	return 0;
}

/**
 * @brief 处理通用的数据
 * @param reg
 * @return int8_t
 */
int8_t ble_proc_general_data(uint8_t *payload)
{
	uint16_t reg = (payload[0] << 8) | payload[1];
	for (uint8_t i = 0; i < ble.general_data_cnt; i++)
	{
		if ((reg == ble.general_data_action[i].reg) && (ble.general_data_action[i].cb != NULL))
		{
			// 执行命令
			ble.general_data_action[i].cb(payload);
		}
	}
	return 0;
}

/**
 * @brief 蓝牙接收重置
 *
 * @param last_rx_len：上次的长度
 */
void ble_rx_reset(uint16_t last_rx_len)
{
	DMA_EnableChannel(BLE_UART_RX_DMA_CH, DISABLE);
	BLE_UART_RX_DMA_CH->TXNUM = BLE_RX_MAX_LEN;
	BLE_UART_RX_DMA_CH->MADDR = (uint32_t)ble.rx_buf;
	memset(ble.rx_buf, 0, last_rx_len);
	ble.rx_cnt = 0;
	DMA_EnableChannel(BLE_UART_RX_DMA_CH, ENABLE);
}

/************************************************注册的蓝牙应用函数******************************************************** */

/**
 * @brief 蓝牙读取信息
 *
 * @param arg
 */
static void ble_read_info(uint8_t *arg)
{
	uint8_t buf_len = 1;
	uint8_t buf[128] = {0};
	uint16_t cmd = (arg[0] << 8) | arg[1]; // 寄存器地址

	buf[buf_len++] = arg[0]; // 寄存器地址高字节
	buf[buf_len++] = arg[1]; // 寄存器地址低字节-
	buf[buf_len++] = 0x00;	 // 数据长度高字节

	switch (cmd)
	{
	// 读取版本号
	case 1:
		buf[buf_len++] = 1; // 数据长度低字节
		buf[buf_len++] = 0x10;
		buf[0] = buf_len - 1;
		break; //   05 00 01 00 01 10

	// 产品型号
	case 2:
		buf[buf_len++] = strlen(about.model);					 // 数据长度低字节
		memcpy(&buf[buf_len], about.model, strlen(about.model)); // 复制数据
		buf_len += strlen(about.model);
		buf[0] = buf_len - 1;
		break;

	// 读取产品硬件版本号
	case 3:
		buf[buf_len++] = 2;
		buf[buf_len++] = about.hardware >> 8;
		buf[buf_len++] = about.hardware & 0xFF;
		buf[0] = buf_len - 1;
		break;

	// 读取产品软件版本号
	case 4:
		buf[buf_len++] = 3;
		buf[buf_len++] = (about.software >> 16) & 0xFF;
		buf[buf_len++] = (about.software >> 8) & 0xFF;
		buf[buf_len++] = about.software & 0xFF;
		buf[0] = buf_len - 1;
		break;

	// 读取产品序列号
	case 5:
		buf[buf_len++] = strlen(about.sn);				   // 数据长度低字节
		memcpy(&buf[buf_len], about.sn, strlen(about.sn)); // 复制数据
		buf_len += strlen(about.sn);
		buf[0] = buf_len - 1;
		break;

	// 读取产品设备大类
	case 6:
		buf[buf_len++] = 1;
		buf[buf_len++] = about.type;
		buf[0] = buf_len - 1;
		break;

	// 读取产品设备次类
	case 7:
		buf[buf_len++] = 1;
		buf[buf_len++] = about.subtype;
		buf[0] = buf_len - 1;
		break;

	// 读取产品总信息(包含1-7的所有数据)
	case 8:
		buf_len = 1; // 重置长度计数器

		// 处理cmd=1数据
		buf[buf_len++] = 0x00; // 寄存器地址高字节(1)
		buf[buf_len++] = 0x01; // 寄存器地址低字节(1)
		buf[buf_len++] = 0x00; // 数据长度高字节
		buf[buf_len++] = 0x01; // 数据长度低字节
		buf[buf_len++] = 0x10; // 数据内容

		// 处理cmd=2数据
		buf[buf_len++] = 0x00; // 寄存器地址高字节(2)
		buf[buf_len++] = 0x02; // 寄存器地址低字节(2)
		uint8_t model_len = strlen(about.model);
		buf[buf_len++] = 0x00;						   // 数据长度高字节
		buf[buf_len++] = model_len;					   // 数据长度低字节
		memcpy(&buf[buf_len], about.model, model_len); // 数据内容
		buf_len += model_len;

		// 处理cmd=3数据
		buf[buf_len++] = 0x00;						   // 寄存器地址高字节(3)
		buf[buf_len++] = 0x03;						   // 寄存器地址低字节(3)
		buf[buf_len++] = 0x00;						   // 数据长度高字节
		buf[buf_len++] = 0x02;						   // 数据长度低字节
		buf[buf_len++] = (about.hardware >> 8) & 0xFF; // 数据内容
		buf[buf_len++] = about.hardware & 0xFF;		   // 数据内容

		// 处理cmd=4数据
		buf[buf_len++] = 0x00;							// 寄存器地址高字节(4)
		buf[buf_len++] = 0x04;							// 寄存器地址低字节(4)
		buf[buf_len++] = 0x00;							// 数据长度高字节
		buf[buf_len++] = 0x03;							// 数据长度低字节
		buf[buf_len++] = (about.software >> 16) & 0xFF; // 数据内容
		buf[buf_len++] = (about.software >> 8) & 0xFF;	// 数据内容
		buf[buf_len++] = about.software & 0xFF;			// 数据内容

		// 处理cmd=5数据
		buf[buf_len++] = 0x00; // 寄存器地址高字节(5)
		buf[buf_len++] = 0x05; // 寄存器地址低字节(5)
		uint8_t sn_len = strlen(about.sn);
		buf[buf_len++] = 0x00;					 // 数据长度高字节
		buf[buf_len++] = sn_len;				 // 数据长度低字节
		memcpy(&buf[buf_len], about.sn, sn_len); // 数据内容
		buf_len += sn_len;

		// 处理cmd=6数据
		buf[buf_len++] = 0x00;		 // 寄存器地址高字节(6)
		buf[buf_len++] = 0x06;		 // 寄存器地址低字节(6)
		buf[buf_len++] = 0x00;		 // 数据长度高字节
		buf[buf_len++] = 0x01;		 // 数据长度低字节
		buf[buf_len++] = about.type; // 数据内容

		// 处理cmd=7数据
		buf[buf_len++] = 0x00;			// 寄存器地址高字节(7)
		buf[buf_len++] = 0x07;			// 寄存器地址低字节(7)
		buf[buf_len++] = 0x00;			// 数据长度高字节
		buf[buf_len++] = 0x01;			// 数据长度低字节
		buf[buf_len++] = about.subtype; // 数据内容

		// 设置总长度
		buf[0] = buf_len - 1;



		break;

	default:
		break;
	}
	ble_send_data_mess(cmd, buf);
}

/**
 * @brief 蓝牙验证信息
 *
 * @param arg
 */
static void ble_verifies_info(uint8_t *arg)
{
	uint8_t buf_len = 0;
	uint8_t ciphertext[4] = {0};
	uint8_t buf[128] = {0};
	uint8_t arg_len = (arg[2] << 8) | arg[3];
	rsa_encryption(ciphertext, arg + 4, arg_len);

	buf_len = 1;

	buf[buf_len++] = arg[0];		// 寄存器地址高字节(1)
	buf[buf_len++] = arg[1];		// 寄存器地址低字节(1)
	buf[buf_len++] = 0x00;			// 数据长度高字节
	buf[buf_len++] = 0x04;			// 数据长度低字节
	buf[buf_len++] = ciphertext[0]; // 数据内容
	buf[buf_len++] = ciphertext[1]; // 数据内容
	buf[buf_len++] = ciphertext[2]; // 数据内容
	buf[buf_len++] = ciphertext[3]; // 数据内容
	buf[0] = buf_len - 1;

	ble_send_data_mess((arg[0] << 8) | arg[1], buf);
}

/**
 * @brief 蓝牙OTA获取固件长度
 *
 * @param arg
 */
static void ble_ota_get_firmware_len(uint8_t *arg)
{
	uint16_t len = (arg[2] << 8) | arg[3]; // 数据长度
	if (len == 0x0004)
	{
		ble.firmware_len = (arg[4] << 24) | (arg[5] << 16) | (arg[6] << 8) | arg[7]; // 获取包的最大长度
	}
	else
	{
		ble.firmware_len = 0;
	}
}

/**
 * @brief 蓝牙OTA固件升级
 *
 * @param arg：payload开始的数据
 */
static void ble_ota_firmware(uint8_t *arg)
{
	LOG_D("ble_OTA");
	uint16_t reg = (arg[0] << 8) | arg[1]; // 寄存器地址
	uint16_t len = (arg[2] << 8) | arg[3]; // 数据长度

	// 写入外部flash

	ble.firmware_len -= len;
	if (ble.firmware_len <= 0)
	{
		// 固件接收完成，重启进入升级
	}
}


static void ble_retransmit_info(uint8_t *arg)
{
	// 重传上一次的发送数据
	ble_prot_retransmission();
}

