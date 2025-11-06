#ifndef __BSP_BLE_H__
#define __BSP_BLE_H__
// clang-format off


#define 	BLE_VDD_PORT 			GPIOB
#define 	BLE_VDD_EN_PIN 			GPIO_PIN_8

#define 	BLE_EN_PORT 			GPIOB
#define 	BLE_EN_PIN 				GPIO_PIN_10

#define 	BLE_STAT_PORT 			GPIOB
#define 	BLE_STAT_PIN 			GPIO_PIN_11

#define 	BLE_RESTORE_PORT 		GPIOB
#define 	BLE_RESTORE_PIN 		GPIO_PIN_9

#define 	BLE_UART_PORT 			GPIOB
#define 	BLE_UART_TX_PIN 		GPIO_PIN_6

#define 	BLE_UART_PORT 			GPIOB
#define 	BLE_UART_RX_PIN 		GPIO_PIN_5


#define 	BLE_TX_UART 			USART1
#define 	BLE_RX_UART 			USART2

#define 	BLE_UART_TX_DMA_CH 		DMA_CH4
#define 	BLE_UART_RX_DMA_CH 		DMA_CH5

#define 	BLE_TX_MAX_LEN 			(532)
#define 	BLE_RX_MAX_LEN 			BLE_TX_MAX_LEN
//===========================================

//===========================================

void bsp_ble_init(void);
void bsp_ble_set_buf(char **_rx_buf, char **_tx_buf);

//===========================================

#endif // __BSP_BLE_H__