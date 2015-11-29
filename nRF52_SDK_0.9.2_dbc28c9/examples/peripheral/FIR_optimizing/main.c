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

#include "arm_math.h"

#define MAX_TEST_DATA_BYTES     (15U)                /**< max number of test bytes to be used for tx and rx. */
#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 1                           /**< UART RX buffer size. */

#define BLOCKSIZE 	32
#define NUM_TAPS 	10
#define FILTERLEN 	10
#define q31_t int

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

int state[BLOCKSIZE];
int coefficents[NUM_TAPS];
int Instance;
int in[32];
int out[32];
int stateIndex;
int state_step2[BLOCKSIZE+NUM_TAPS];

void fir(q31_t *in, q31_t *out, q31_t *coeffs, int *stateIndexPtr, 
		int filtLen, int blockSize)
{
int sample;
int k;
q31_t sum;
int stateIndex = *stateIndexPtr;
	
	for(sample=0; sample < blockSize; sample++)
    {
		state[stateIndex++] = in[sample];
		sum=0;
		for(k=0;k<filtLen;k++)
		{
			sum += coeffs[k] * state[stateIndex];
			stateIndex--;
			if (stateIndex < 0) 
			{
				stateIndex = filtLen-1;
			}
		}
		out[sample]=sum;
    }
	*stateIndexPtr = stateIndex;
}

void fir_block(q31_t *in, q31_t *out, q31_t *coeffs, int *stateIndexPtr, 
		int filtLen, int blockSize)
{
int sample;
int k;
q31_t sum;
int stateIndex = *stateIndexPtr;
	
	for(sample=0; sample < blockSize; sample++)
    {
		state_step2[stateIndex++] = in[sample];
		sum=0;
		for(k=0; k<filtLen; k++)
		{
			sum += coeffs[k] * state_step2[stateIndex];
			stateIndex++;
		}
		out[sample]=sum;
    }
	*stateIndexPtr = stateIndex;
}

void fir_unrolling(q31_t *in, q31_t *out, q31_t *coeffs, int *stateIndexPtr, 
		int filtLen, int blockSize)
{
int sample;
int k;
q31_t sum;
int stateIndex = *stateIndexPtr;
	
	for(sample=0; sample < blockSize; sample++)
    {
		state[stateIndex++] = in[sample];
		sum=0;
		k = filtLen >>2;
		for(k=0;k<filtLen;k++)
		{
			sum += coeffs[k] * state_step2[stateIndex];
			stateIndex++;
			sum += coeffs[k] * state_step2[stateIndex];
			stateIndex++;
			sum += coeffs[k] * state_step2[stateIndex];
			stateIndex++;
			sum += coeffs[k] * state_step2[stateIndex];
			stateIndex++;
		}
		out[sample]=sum;
		*stateIndexPtr = stateIndex;
	}
}

void fir_SIMD(q31_t *in, q31_t *out, q31_t *coeffs, int *stateIndexPtr, 
		int filtLen, int blockSize)
{
int sample,I;
int k,c,s;
q31_t sum;
int stateIndex = *stateIndexPtr;
	
	for(sample=0; sample < blockSize; sample++)
    {
		state[stateIndex++] = in[sample];
		sum=0;
		I = 0;
		k = filtLen >>2;
		for(k=0;k<filtLen;k++)
		{
			c = *coeffs++;				
			s = state[I++];				
			sum = __SMLALD(c, s, sum);		
			c = *coeffs++;				
			s = state[I++];					
			sum = __SMLALD(c, s, sum);		
			c = *coeffs++;				
			s = state[I++];					
			sum = __SMLALD(c, s, sum);		
			c = *coeffs++;				
			s = state[I++];					
			sum = __SMLALD(c, s, sum);		
		}					
	}
	out[sample]=sum;
    *stateIndexPtr = stateIndex;
}


void fir_SuperUnrolling(q31_t *in, q31_t *out, q31_t *coeffs, int *stateIndexPtr, 
		int filtLen, int blockSize)
{
int sample,i;
int c0,x0,x1,x2,x3,sum0,sum1,sum2,sum3;
int stateIndex = *stateIndexPtr;
int * coeffPtr;
	
	sample = blockSize/4;
	do
	{
		sum0 = sum1 = sum2 = sum3 = 0;
		stateIndex = *stateIndexPtr;
		coeffPtr = coeffs;
		x0 = *(q31_t *)(stateIndex++);
		x1 = *(q31_t *)(stateIndex++);
		i = NUM_TAPS>>2;	
		do	
		{
			c0 = *(coeffPtr++);
			x2 = *(q31_t *)(stateIndex++);
			x3 = *(q31_t *)(stateIndex++);
			sum0  = __SMLALD(x0, c0, sum0);
			sum1  = __SMLALD(x1, c0, sum1);
			sum2  = __SMLALD(x2, c0, sum2);
			sum3  = __SMLALD(x3, c0, sum3);
			c0 = *(coeffPtr++);
			x0 = *(q31_t *)(stateIndex++);
			x1 = *(q31_t *)(stateIndex++);
			sum0  = __SMLALD(x0, c0, sum0);
			sum1  = __SMLALD(x1, c0, sum1);
			sum2  = __SMLALD(x2, c0, sum2);
			sum3  = __SMLALD(x3, c0, sum3);
			} while(--i);	
		out[sample] = sum0;		
		out[sample+1] = sum1; 	
		out[sample+2] = sum2;
		out[sample+3] = sum3;
		stateIndexPtr= stateIndexPtr + 4;
	} while(--sample);

}

/**
 * @brief Function for main application entry.
 */
int main(void)
{
	// Joseph Yiu's method
	int cyc[2];
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

	STOPWATCH_START;
	fir(in,out,coefficents,&stateIndex,FILTERLEN, BLOCKSIZE);
	STOPWATCH_STOP;
	printf("\n\r%d cycles for fir() \n\r",cyc[1]);
	
	STOPWATCH_START;
	fir_block(in,out,coefficents,&stateIndex,FILTERLEN, BLOCKSIZE);
	STOPWATCH_STOP;
	printf("\n\r%d cycles for fir_block() \n\r",cyc[1]);
	
	STOPWATCH_START;	
	fir_unrolling(in,out,coefficents,&stateIndex,FILTERLEN, BLOCKSIZE);
	STOPWATCH_STOP;
	printf("\n\r%d cycles for fir_unrolling() \n\r",cyc[1]);	
	
	STOPWATCH_START;
	fir_SIMD(in,out,coefficents,&stateIndex,FILTERLEN, BLOCKSIZE);
	STOPWATCH_STOP;
	printf("\n\r%d cycles for fir_SIMD() \n\r",cyc[1]);

	STOPWATCH_START;
	fir_SuperUnrolling(in,out,coefficents,&stateIndex,FILTERLEN, BLOCKSIZE);
	STOPWATCH_STOP;
	printf("\n\r%d cycles for fir_SuperUnrolling \n\r",cyc[1]);
	
	while (true) {}
}


/** @} */
