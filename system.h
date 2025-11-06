#ifndef __SYSTEM_H__
#define __SYSTEM_H__
// clang-format off

// 库
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

// MCU
#include "n32g45x.h"

// rt-thread
#include "rtthread.h"
#include "SEGGER_RTT.h"
#include "mlog.h"

// 驱动
#include "bsp_key.h"
#include "bsp_lcd_spi.h"
#include "bsp_dac_sin.h"
#include "bsp_adc_vrms.h"
#include "bsp_cs1237.h"
#include "bsp_led_beep.h"
#include "bsp_flash.h"
#include "bsp_lcd_spi.h"

// USB驱动
#include "hw_config.h"
#include "usb_regs.h"
#include "usb_sil.h"

// 数据
#include "data_proc.h"

//===========================================

#define 	FLASH_BASE_ADDR 				0x08000000		// FLASH起始地址
#define 	FLASH_SIZE 						0x00080000		// FLASH大小，512KB

#define 	UID_BASE_ADDR 					0x1FFFF7E8		// UID起始地址（共12字节）
#define 	CALIBRATION_DATA_ADDR 			0X08005000	  	// 校准数据存储地址（只读，不可随意修改）

#define 	STEP_DELAY_MS					50

// 系统上电引脚
#define	 	ON_KEY_PORT 					GPIOB
#define	 	ON_KEY_PIN 						GPIO_PIN_4
#define	 	LOCK_PORT 						GPIOB
#define	 	LOCK_PIN 						GPIO_PIN_5

#define     pin_on(port, pins)      		port->PBSC = pins
#define     pin_off(port, pins)     		port->PBC = pins
#define     pin_toggle(port, pins)  		port->POD ^= (pins)

#define     pin_read(port, pins)  			(port->PID & pins)

//===========================================



//===========================================

void system_init(void);

#endif // __SYSTEM_H__