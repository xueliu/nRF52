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
#include <math.h>
#include "boards.h"                              

volatile int a1;

/**
 * @brief Function for application main entry.
 */
int main (void)
{
float a,b,c,d,e;
int f,g = 100;

	while(1)
	{
		a= 10.1234;
		b=100.2222;
		c= a*b;
		d= c-a;
		e= d+b;
		f =(int)a;
		f = f*g;
		a1 = (unsigned int) a;
//		a=__sqrtf(e); 			//Square root by Hardware FPU 
		a= sqrt(e);				//Square root by software library
		a = c/f;	
		e = a/0;				//Raise divide by zero error
	}
}



/** @} */
