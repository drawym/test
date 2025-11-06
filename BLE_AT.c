#include "system.h"

/*Ble_at.c*/
/*
NAME	：蓝牙名称
LADDR	：MAC地址
BAUD	：波特率
DISC	：断开连接
RESET	：重置
UUID	：UUID
TEADV	：设置广播内容
NOTI	：通知
ADVI	：广播时间
*/

// clang-format off
/**
 * @brief 蓝牙广播数据包
 * 格式说明：
 * [0] = 28 (总长度)
 * [1-4] = 0x03, 0x03, 0xFE, 0xCC (标志位和UUID)
 * [5-22] = 0x0F, 0x09, ... (设备名称和序列号，共18字节)
 * [23-28] = 0x07, 0xFF, ... (MAC地址，共6字节)
 */
/*这个怎么获得，通过读取UUID然后再固定写入吗？*/
/*是  长度  type  描述  ！！！*/

uint8_t ble_adv_data[29] = {
	28,
	0x03, 0x03, 0xFE, 0xCC,
	0x0F, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x07, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// clang-format on

//=======================================函数声明========================================

static void	  ble_send_at_cmd(char *cmd, void *arg, ARG_TYPE_t arg_type);
static int8_t ble_proc_at_cmd(char *cmd, rt_int32_t timeout);

static int8_t hexstr_to_hexarray(char *s, uint8_t *hex);
static int8_t hexarray_to_hexstr(const uint8_t *hex, int len, char *out);

static void ble_set_ble_adv_name_sn(char *name, char *sn);
static void ble_set_ble_adv_mac(char *mac);

//=======================================函数定义========================================

/**
 * @brief 蓝牙配置主函数
 * 完成蓝牙模块的初始化配置，包括名称、UUID、MAC地址、广播数据等
 */
void ble_config(void)
{
	rt_event_control(ble.event, RT_IPC_CMD_RESET, RT_NULL);
	ble_rx_reset(BLE_RX_MAX_LEN);

	// 配置蓝牙名称
	ble_at_send("NAME", "HRM-20-123456", ARG_TYPE_STRING, 1000);
	rt_thread_delay(100);

	// 配置UUID--作用？
	char uuid[7] = {0};
	sprintf(uuid, "0x%04X", 0xFECC);
	ble_at_send("UUID", uuid, ARG_TYPE_STRING, 1000);
	rt_thread_delay(100);

	// 读取蓝牙的mac地址
	ble_at_send("LADDR", NULL, ARG_TYPE_NONE, 1000);
	rt_thread_delay(100);

	// 配置广播内容
	ble_set_ble_adv_mac(ble.at_rx_param);
	ble_set_ble_adv_name_sn("HRM-20", "123456");
	ble_at_send("TEADV", ble_adv_data, ARG_TYPE_HEX_ARRAY, 1000);
	rt_thread_delay(100);

	while (ble_proc_at_cmd("TTM:CONNECTED", RT_WAITING_FOREVER) != 0);

	// 等待连接
	while (1)
	{
		rt_thread_delay(10);
		if (GPIO_ReadInputDataBit(BLE_STAT_PORT, BLE_STAT_PIN) == 0)
		{
			rt_event_send(ble.event, EVENT_BLE_CON);
			ble_rx_reset(BLE_RX_MAX_LEN);
			ble.connect_states = 1; // 设置连接状态
			return;
		}
	}
}

/**
 * @brief 发送AT指令的公共接口函数
 * 支持重试机制，最多重试3次
 * @param cmd AT指令字符串
 * @param arg 参数指针
 * @param arg_type 参数类型
 * @param timeout 超时时间(毫秒)
 * @return 0成功，-1失败
 */
int8_t ble_at_send(char *cmd, void *arg, ARG_TYPE_t arg_type, rt_int32_t timeout)
{
	char response[10] = {0};
	sprintf(response, "+%s", cmd);    // +UUID
	for (uint8_t i = 0; i < 3; i++)
	{
		ble_send_at_cmd(cmd, arg, arg_type);
		if (ble_proc_at_cmd(response, timeout) == 0)   // +LADDR
		{
			return 0;
		}
		rt_thread_delay(100);
	}
	return -1;
}

/**
 * @brief 发送AT指令
 *
 * @param cmd：AT指令
 * @param arg：携带参数
 * @param arg_type：参数类型
 */
static void ble_send_at_cmd(char *cmd, void *arg, ARG_TYPE_t arg_type)
{
	// 填充"AT+"前缀
	ble.tx_buf[0] = 'A';
	ble.tx_buf[1] = 'T';
	ble.tx_buf[2] = '+';

	// 复制 cmd 到 (tx_buf+3) 的位置
	uint8_t cmd_len = strlen(cmd);
	memcpy(ble.tx_buf + 3, cmd, cmd_len);   //AT+UUID
	uint16_t total_len = 3 + cmd_len; // 当前总长度 (包含"AT+"和cmd)

	// 处理不同类型的参数
	if (arg != NULL && arg_type != ARG_TYPE_NONE)
	{
		int written = 0;
		switch (arg_type)
		{
			case ARG_TYPE_STRING: // 字符串参数
				written = sprintf((char *)(ble.tx_buf + total_len), "%s", (char *)arg);  //AT+UUIDFECC
				break;

			case ARG_TYPE_INT: // 整型参数
				written = sprintf((char *)(ble.tx_buf + total_len), "%d", *(int *)arg);
				break;

			case ARG_TYPE_HEX_ARRAY: // 十六进制数组参数
				written = hexarray_to_hexstr((uint8_t *)((uint8_t *)arg + 1), *(uint8_t *)arg, (char *)(ble.tx_buf + total_len));
				break;

			default:
				written = 0;
				break;
		}
		total_len += written;
	}

	// 4. 添加AT指令结束符\r\n
	ble.tx_buf[total_len++] = '\r';
	ble.tx_buf[total_len++] = '\n';

	ble_send_data(total_len);
}

/**
 * @brief 处理AT指令响应的内部函数
 * 等待并解析蓝牙模块的响应
 * @param cmd 期望的响应字符串
 * @param timeout 超时时间
 * @return 0成功，-1失败
 */
static int8_t ble_proc_at_cmd(char *cmd, rt_int32_t timeout)
{
	int8_t		ret		   = -1;
	uint8_t		cmd_len	   = strlen(cmd);
	rt_uint32_t recved	   = 0;
	uint8_t		rx_buf_len = 0;

	if (rt_event_recv(ble.event, ALL_EVENT, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, timeout, &recved) != RT_EOK)
	{
		goto over;
	}

	LOG_D("cmd:%s", cmd);

	if ((recved & EVENT_RECV) == EVENT_RECV)
	{
		if ((*cmd) == '+')
		{
			rx_buf_len = strlen(ble.rx_buf);
			// 遍历"+"
			for (uint8_t i = 0; i < rx_buf_len; i++)
			{
				if (ble.rx_buf[i] == '+')
				{
					// 检查是否匹配期望的响应
					if (strncmp((ble.rx_buf + i), cmd, cmd_len) == 0)
					{
						if (rx_buf_len >= (cmd_len + 1))
						{
							uint8_t param_offset = i + cmd_len + 1;// 参数开始位置
							for (uint8_t j = param_offset; j < rx_buf_len; j++)
							{
								if (ble.rx_buf[j] == '\r')
								{
									// 提取参数
									uint8_t param_len = j - param_offset;
									memcpy(ble.at_rx_param, (ble.rx_buf + param_offset), param_len);
									ble.at_rx_param[param_len] = '\0'; //  null-terminate
									LOG_D("param: %s", ble.at_rx_param);
								}
							}
						}
						ret = 0;
						goto over;
					}
				}
			}
		}
		else if ((*cmd) == 'T')
		{
			rx_buf_len = strlen(ble.rx_buf);
			// 遍历"+"
			for (uint8_t i = 0; i < rx_buf_len; i++)
			{
				if (ble.rx_buf[i] == 'T')
				{
					if (strncmp((ble.rx_buf + i), cmd, cmd_len) == 0)
					{
						LOG_D("CONNECTED");
						ret = 0;
						goto over;
					}
				}
			}
		}
	}
over:
	ble_rx_reset(rx_buf_len);
	return ret;
}

//=======================================工具函数========================================

/**
 * @brief 蓝牙设置广播信息名字和SN
 *
 */
static void ble_set_ble_adv_name_sn(char *name, char *sn)
{
	uint8_t len = strlen(name);
	memcpy(ble_adv_data + 7, name, len);
	ble_adv_data[7 + len] = '-';
	len					  = strlen(sn);
	memcpy((ble_adv_data + 8 + len), sn, len);
}

/**
 * @brief 设置蓝牙广播信息mac地址
 *
 * @param mac
 */
static void ble_set_ble_adv_mac(char *mac)
{
	uint8_t temp_mac[6] = {0};
	hexstr_to_hexarray(mac, temp_mac);
	memcpy(ble_adv_data + 23, temp_mac, 6);
}

/**
 * @brief 将 "1234" 这样的字符串转成 {0x12, 0x34} 格式的字节数组
 * @param s     输入字符串（比如 "1234"）
 * @param hex   输出的字节数组
 * @return 写入的字节数（成功）或 -1（失败）
 */
static int8_t hexstr_to_hexarray(char *s, uint8_t *hex)
{
	int cnt = 0;

	while (s[0] && s[1]) // 保证剩余字符至少2位
	{
		// 转换高位字节
		uint8_t high = (s[0] <= '9') ? (s[0] - '0') : (toupper(s[0]) - 'A' + 10);

		// 转换低位字节
		uint8_t low = (s[1] <= '9') ? (s[1] - '0') : (toupper(s[1]) - 'A' + 10);

		// 组合成完整字节
		hex[cnt] = (high << 4) | low;

		cnt++;
		s += 2; // 推进指针，解析下一组2字符
	}

	// 如果输入字符串有剩余字符（奇数长度），则失败
	if (*s != '\0')
		return -1;

	return cnt;
}

/**
 * @brief 	将16进制数组转换为字符串
 * 			例如：0x12 0x34，转为字符串"1234"
 *
 * @param hex：16进制数组
 * @param len：数组长度
 * @param out：输出字符串
 */
static int8_t hexarray_to_hexstr(const uint8_t *hex, int len, char *out)
{
	int8_t			  cnt	= 0;
	static const char tbl[] = "0123456789ABCDEF";

	for (int i = 0; i < len; ++i)
	{
		out[i * 2]	   = tbl[hex[i] >> 4];	 // 高 4 位
		out[i * 2 + 1] = tbl[hex[i] & 0x0F]; // 低 4 位
		cnt += 2;
	}
	out[len * 2] = '\0'; // 结尾 0
	return cnt;
}
