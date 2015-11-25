/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
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
 * @defgroup uart_example_main main.c
 * @{
 * @ingroup uart_example
 * @brief UART Example Application main file.
 *
 * This file contains the source code for a sample application using UART.
 * 
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_uart.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf.h"
#include "bsp.h"

#define MAX_TEST_DATA_BYTES     (15U)                /**< max number of test bytes to be used for tx and rx. */
#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 1                           /**< UART RX buffer size. */

void uart_error_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

/**
 * @brief Function for main application entry.
 */
int main(void)
{
	// Joseph Yiu's method
	int cyc[2];
	double x;
	volatile uint32_t count = 0;
	
	uint32_t err_code;
    const app_uart_comm_params_t comm_params =
      {
          RX_PIN_NUMBER,
          TX_PIN_NUMBER,
          RTS_PIN_NUMBER,
          CTS_PIN_NUMBER,
          APP_UART_FLOW_CONTROL_ENABLED,
          false,
          UART_BAUDRATE_BAUDRATE_Baud38400
      };

    APP_UART_FIFO_INIT(&comm_params,
                         UART_RX_BUF_SIZE,
                         UART_TX_BUF_SIZE,
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOW,
                         err_code);

    APP_ERROR_CHECK(err_code);
	  
	// enable the use DWT
	CoreDebug->DEMCR |= 0x01000000;

	// Reset cycle counter
	DWT->CYCCNT = 0;

	// enable cycle counter
	DWT->CTRL |= 0x1;
	
	#define STOPWATCH_START { cyc[0] = DWT->CYCCNT;}
	#define STOPWATCH_STOP { cyc[1] = DWT->CYCCNT; cyc[1] = cyc[1] - cyc[0]; }
		  
	// some code here
	// .....
	
	x = 10.0;

	STOPWATCH_START;
	x *= 10.0;
	x *= 10.0;
	x *= 10.0;
	x *= 10.0;
	x *= 10.0;
	STOPWATCH_STOP;

	printf("\n\r%d cycles for 5 double multiplies\n\r",cyc[1]);
	
	while (true) {}
}


/** @} */
