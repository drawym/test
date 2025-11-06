#include "system.h"

/*bsp_ble.c*/

static uint8_t tx_buf[BLE_TX_MAX_LEN];
static uint8_t rx_buf[BLE_RX_MAX_LEN];

//============================================= 函数声明 =============================================

static void bsp_ble_gpio_init(void);
static void bsp_ble_uart_init(void);
static void bsp_ble_dma_init(void);

//============================================= 函数定义 =============================================

/**
 * @brief 初始化蓝牙模块
 *
 */
void bsp_ble_init(void)
{
	bsp_ble_gpio_init();
	bsp_ble_dma_init();
	bsp_ble_uart_init();
}

/**
 * @brief 初始化gpio
 *
 */
static void bsp_ble_gpio_init(void)
{
	GPIO_InitType gpio_initstructure;

	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);

	// TX
	GPIO_InitStruct(&gpio_initstructure);
	gpio_initstructure.Pin			  = BLE_UART_TX_PIN;
	gpio_initstructure.GPIO_Pull	  = GPIO_Pull_Up;
	gpio_initstructure.GPIO_Mode	  = GPIO_Mode_AF_PP; // 复用输出
	gpio_initstructure.GPIO_Alternate = GPIO_AF0_USART1;
	GPIO_InitPeripheral(BLE_UART_PORT, &gpio_initstructure);

	// RX
	GPIO_InitStruct(&gpio_initstructure);
	gpio_initstructure.Pin			  = BLE_UART_RX_PIN;
	gpio_initstructure.GPIO_Alternate = GPIO_AF6_USART2;
	GPIO_InitPeripheral(BLE_UART_PORT, &gpio_initstructure);

	// 使能EN
	GPIO_InitStruct(&gpio_initstructure);
	gpio_initstructure.Pin			  = BLE_EN_PIN;
	gpio_initstructure.GPIO_Slew_Rate = GPIO_Slew_Rate_High;
	gpio_initstructure.GPIO_Pull	  = GPIO_No_Pull;
	gpio_initstructure.GPIO_Mode	  = GPIO_Mode_Out_PP; // 复用输出
	gpio_initstructure.GPIO_Current	  = GPIO_DC_12mA;
	GPIO_InitPeripheral(BLE_EN_PORT, &gpio_initstructure);
	GPIO_ResetBits(BLE_EN_PORT, BLE_EN_PIN); // = 0，使能

	// 复位
	gpio_initstructure.Pin = BLE_RESTORE_PIN;
	GPIO_InitPeripheral(BLE_RESTORE_PORT, &gpio_initstructure);
	GPIO_SetBits(BLE_RESTORE_PORT, BLE_RESTORE_PIN);

	// ble电源使能
	GPIO_InitStruct(&gpio_initstructure);
	gpio_initstructure.Pin			  = BLE_VDD_EN_PIN;
	gpio_initstructure.GPIO_Slew_Rate = GPIO_Slew_Rate_High;
	gpio_initstructure.GPIO_Pull	  = GPIO_No_Pull;
	gpio_initstructure.GPIO_Mode	  = GPIO_Mode_Out_PP; // 复用输出
	gpio_initstructure.GPIO_Current	  = GPIO_DC_12mA;
	GPIO_InitPeripheral(BLE_VDD_PORT, &gpio_initstructure);
	GPIO_SetBits(BLE_VDD_PORT, BLE_VDD_EN_PIN); // = 1，使能

	// ble连接状态
	GPIO_InitStruct(&gpio_initstructure);
	gpio_initstructure.Pin		 = BLE_STAT_PIN;
	gpio_initstructure.GPIO_Pull = GPIO_Pull_Up;
	gpio_initstructure.GPIO_Mode = GPIO_Mode_Input; // 复用输出
	GPIO_InitPeripheral(BLE_STAT_PORT, &gpio_initstructure);
}

/**
 * @brief 初始化蓝牙串口dma
 *
 */
static void bsp_ble_dma_init(void)
{
	DMA_InitType  dma_initstructure;
	NVIC_InitType nvic_initstructure;

	nvic_initstructure.NVIC_IRQChannel					 = DMA_Channel4_IRQn;
	nvic_initstructure.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstructure.NVIC_IRQChannelSubPriority		 = 5;
	nvic_initstructure.NVIC_IRQChannelCmd				 = ENABLE;
	NVIC_Init(&nvic_initstructure);

	RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_DMA, ENABLE);

	DMA_DeInit(BLE_UART_TX_DMA_CH);
	DMA_StructInit(&dma_initstructure);
	dma_initstructure.PeriphAddr	 = (uint32_t)&BLE_TX_UART->DAT;
	dma_initstructure.MemAddr		 = (uint32_t)tx_buf;
	dma_initstructure.Direction		 = DMA_DIR_PERIPH_DST;
	dma_initstructure.BufSize		 = 100;
	dma_initstructure.PeriphInc		 = DMA_PERIPH_INC_DISABLE;
	dma_initstructure.DMA_MemoryInc	 = DMA_MEM_INC_ENABLE;
	dma_initstructure.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
	dma_initstructure.MemDataSize	 = DMA_MemoryDataSize_Byte;
	dma_initstructure.CircularMode	 = DMA_MODE_NORMAL;
	dma_initstructure.Priority		 = DMA_PRIORITY_VERY_HIGH;
	dma_initstructure.Mem2Mem		 = DMA_M2M_DISABLE;
	DMA_Init(BLE_UART_TX_DMA_CH, &dma_initstructure);
	DMA_RequestRemap(DMA_REMAP_USART1_TX, DMA, BLE_UART_TX_DMA_CH, ENABLE);
	DMA_ClearFlag(DMA_FLAG_TC4, DMA);
	DMA_ClrIntPendingBit(DMA_INT_TXC4, DMA);
	DMA_ConfigInt(BLE_UART_TX_DMA_CH, DMA_INT_TXC, ENABLE);

	// 接收
	DMA_DeInit(BLE_UART_RX_DMA_CH);
	dma_initstructure.PeriphAddr	 = (uint32_t)&BLE_RX_UART->DAT;
	dma_initstructure.MemAddr		 = (uint32_t)rx_buf;
	dma_initstructure.Direction		 = DMA_DIR_PERIPH_SRC;
	dma_initstructure.BufSize		 = BLE_RX_MAX_LEN;
	dma_initstructure.PeriphInc		 = DMA_PERIPH_INC_DISABLE;
	dma_initstructure.DMA_MemoryInc	 = DMA_MEM_INC_ENABLE;
	dma_initstructure.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
	dma_initstructure.MemDataSize	 = DMA_MemoryDataSize_Byte;
	dma_initstructure.CircularMode	 = DMA_MODE_CIRCULAR;
	dma_initstructure.Priority		 = DMA_PRIORITY_VERY_HIGH;
	dma_initstructure.Mem2Mem		 = DMA_M2M_DISABLE;
	DMA_Init(BLE_UART_RX_DMA_CH, &dma_initstructure);
	DMA_RequestRemap(DMA_REMAP_USART2_RX, DMA, BLE_UART_RX_DMA_CH, ENABLE);

	DMA_EnableChannel(BLE_UART_TX_DMA_CH, ENABLE);
	DMA_EnableChannel(BLE_UART_RX_DMA_CH, ENABLE);
}

/**
 * @brief 初始化uart
 *
 */
static void bsp_ble_uart_init(void)
{
	USART_InitType usart_initstructure;
	NVIC_InitType  nvic_initstructure;

	nvic_initstructure.NVIC_IRQChannel					 = USART2_IRQn;
	nvic_initstructure.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstructure.NVIC_IRQChannelSubPriority		 = 5;
	nvic_initstructure.NVIC_IRQChannelCmd				 = ENABLE;
	NVIC_Init(&nvic_initstructure);

	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_USART1, ENABLE);
	RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_USART2, ENABLE);

	USART_StructInit(&usart_initstructure);
	usart_initstructure.BaudRate			= 115200;
	usart_initstructure.WordLength			= USART_WL_8B;
	usart_initstructure.StopBits			= USART_STPB_1;
	usart_initstructure.Parity				= USART_PE_NO;
	usart_initstructure.HardwareFlowControl = USART_HFCTRL_NONE;
	usart_initstructure.Mode				= USART_MODE_TX;
	USART_Init(BLE_TX_UART, &usart_initstructure);

	usart_initstructure.Mode = USART_MODE_RX;
	USART_Init(BLE_RX_UART, &usart_initstructure);

	USART_EnableDMA(BLE_TX_UART, USART_DMAREQ_TX, ENABLE);
	USART_EnableDMA(BLE_RX_UART, USART_DMAREQ_RX, ENABLE);

	USART_ConfigInt(BLE_RX_UART, USART_INT_IDLEF, ENABLE); // 接收空闲中断

	// 清除接收中断标志位
	USART_GetIntStatus(BLE_RX_UART, USART_INT_IDLEF);
	USART_ReceiveData(BLE_RX_UART);
}

/**
 * @brief
 *
 * @param rx_buf
 * @param tx_buf
 */
void bsp_ble_set_buf(char **_rx_buf, char **_tx_buf)
{
	(*_rx_buf) = (char *)rx_buf;
	(*_tx_buf) = (char *)tx_buf;
}
