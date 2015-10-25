/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 * Copyright (c) 2015 xue liu <liuxuenetmail@gmail.com>. All Rights Reserved.
 
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 *
 * @defgroup systick_blinky_example_main main.c
 * @{
 * @ingroup systick_blinky_example
 * @brief Systick Blinky Example Application main file.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"                              

#define SYSTICK_COUNT_ENABLE		1
#define SYSTICK_INTERRUPT_ENABLE	2

uint32_t msTicks = 0; /* Variable to store millisecond ticks */

/* SysTick interrupt Handler. */
void SysTick_Handler(void)  {
	if(++msTicks == 500) {
		LEDS_INVERT(BSP_LED_1_MASK); /* light LED 2 very 1 second */
		msTicks = 0;
	}
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    // Configure LED 2 as outputs.
    LEDS_CONFIGURE(BSP_LED_1_MASK);

	uint32_t returnCode;
	returnCode = SysTick_Config(SystemCoreClock / 1000);      /* Configure SysTick to generate an interrupt every millisecond */ 
	if (returnCode != 0)  {                                   /* Check return code for errors */
	// Error Handling 
	}
	
	// The following code has the same effect
	//	SysTick->VAL   = 640000UL;								//Start value for the sys Tick counter
	//	SysTick->LOAD  = 640000UL;								//Reload value 
	//	SysTick->CTRL = SYSTICK_INTERRUPT_ENABLE
	//					|SYSTICK_COUNT_ENABLE;					//Start and enable interrupt
	while(1);
}


/** @} */
