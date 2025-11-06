#include "system.h"

RCC_ClocksType RCC_ClockFreq;

int main(void)
{
	uint8_t cnt = 0;
	LOG_D("system start...");
	system_init();

	ble_init();

	RCC_GetClocksFreqValue(&RCC_ClockFreq);
	// LOG_D("SYSCLK: %d", RCC_ClockFreq.SysclkFreq);
	// LOG_D("HCLK: %d", RCC_ClockFreq.HclkFreq);
	// LOG_D("PCLK1: %d", RCC_ClockFreq.Pclk1Freq);
	// LOG_D("PCLK2: %d", RCC_ClockFreq.Pclk2Freq);
	// LOG_D("ADC: %d", RCC_ClockFreq.AdcHclkFreq);

	while (1)
	{
		rt_thread_delay(4000);
		// LOG_D("system runing...");
	}
}