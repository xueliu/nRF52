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
 * @defgroup dwt_example_main main.c
 * @{
 * @ingroup dwt_example
 * @brief Data Watchpoint and Trace Example Application main file.
 *
 */

#include "nrf.h"
#include "nrf_gpio.h"
#include "boards.h"
#include <stdint.h>

/**
 * @brief Function for application main entry.
 */
int main (void)
{
	volatile uint32_t count = 0;

	// enable the use DWT
	CoreDebug->DEMCR |= 0x01000000;

	// Reset cycle counter
	DWT->CYCCNT = 0;

	// enable cycle counter
	DWT->CTRL |= 0x1; 

	// some code here
	// .....

	// number of cycles stored in count variable
	count = DWT->CYCCNT;
}



/** @} */
