#include "system.h"

/*system.c*/
uint8_t peripheral_ret = 0; // bit0 = csd204m，bit1 = fusb302，bit2 = kxtj3_1057，bit3 = w25q64

//============================================= 函数声明 =============================================

static void system_power_on(void);

//============================================= 函数定义 =============================================

/**
 * @brief 系统初始化
 *
 */
void system_init(void)
{
	system_power_on();
}

/**
 * @brief 系统上电
 */
static void system_power_on(void)
{
	GPIO_InitType gpio_initstructure;

	RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOC, ENABLE);
	GPIO_InitStruct(&gpio_initstructure);
	gpio_initstructure.Pin			  = ON_KEY_PIN;
	gpio_initstructure.GPIO_Slew_Rate = GPIO_Slew_Rate_High;
	gpio_initstructure.GPIO_Pull	  = GPIO_Pull_Up;
	gpio_initstructure.GPIO_Mode	  = GPIO_Mode_Input;
	gpio_initstructure.GPIO_Current	  = GPIO_DC_12mA;
	GPIO_InitPeripheral(ON_KEY_PORT, &gpio_initstructure);

	GPIO_InitStruct(&gpio_initstructure);
	gpio_initstructure.Pin			  = LOCK_PIN;
	gpio_initstructure.GPIO_Slew_Rate = GPIO_Slew_Rate_High;
	gpio_initstructure.GPIO_Pull	  = GPIO_No_Pull;
	gpio_initstructure.GPIO_Mode	  = GPIO_Mode_Out_PP;
	gpio_initstructure.GPIO_Current	  = GPIO_DC_12mA;
	GPIO_InitPeripheral(LOCK_PORT, &gpio_initstructure);

	if (GPIO_ReadInputDataBit(ON_KEY_PORT, ON_KEY_PIN) != Bit_SET)
		pin_on(LOCK_PORT, LOCK_PIN);
}

/**
 * @brief 滴答定时器初始化
 *
 */
void systick_init(uint32_t tick)
{
	NVIC_InitType NVIC_InitStructure;

	// 配置systick时钟
	SysTick->CTRL = 0;
	SysTick->LOAD = (tick)-1;
	SysTick->VAL  = 0;
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
	SysTick->CTRL |= 0x00000003;

	// systick中断初始化
	NVIC_InitStructure.NVIC_IRQChannel					 = SysTick_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 15;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority		 = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd				 = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}