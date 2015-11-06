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

#define BTN_PRESSED 0
#define BTN_RELEASED 1

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
//    // Configure LED 2 as outputs.
//    LEDS_CONFIGURE(BSP_LED_1_MASK);

//	uint32_t returnCode;
//	returnCode = SysTick_Config(SystemCoreClock / 1000);      /* Configure SysTick to generate an interrupt every millisecond */ 
//	if (returnCode != 0)  {                                   /* Check return code for errors */
//	// Error Handling 
//	}
//	
//	
//	
//	// The following code has the same effect
//	//	SysTick->VAL   = 640000UL;								//Start value for the sys Tick counter
//	//	SysTick->LOAD  = 640000UL;								//Reload value 
////		SysTick->CTRL = SYSTICK_INTERRUPT_ENABLE
////						|SYSTICK_COUNT_ENABLE;					//Start and enable interrupt
//	while(1) {
//    /* Clear Event Register */
//    __SEV();
//    /* Wait for event */
//    __WFE();
//    /* Wait for event */
//    __WFE();
//	}
    // Configure BUTTON0 as a regular input
    nrf_gpio_cfg_input(BSP_BUTTON_1, NRF_GPIO_PIN_NOPULL);
    
    // Configure BUTTON1 with SENSE enabled
    nrf_gpio_cfg_sense_input(BSP_BUTTON_1, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_LOW);
    
    // Configure the LED pins as outputs
    nrf_gpio_range_cfg_output(LED_START, LED_STOP);
    
    nrf_gpio_pin_set(BSP_LED_1);
    
    // Internal 32kHz RC
    NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_RC << CLOCK_LFCLKSRC_SRC_Pos;
    
    // Start the 32 kHz clock, and wait for the start up to complete
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    while(NRF_CLOCK->EVENTS_LFCLKSTARTED == 0);
    
    // Configure the RTC to run at 2 second intervals, and make sure COMPARE0 generates an interrupt (this will be the wakeup source)
    NRF_RTC1->PRESCALER = 0;
    NRF_RTC1->EVTENSET = RTC_EVTEN_COMPARE0_Msk; 
    NRF_RTC1->INTENSET = RTC_INTENSET_COMPARE0_Msk; 
    NRF_RTC1->CC[0] = 2*32768;
    NVIC_EnableIRQ(RTC1_IRQn);
            
    // Configure the RAM retention parameters
    NRF_POWER->RAMON = POWER_RAMON_ONRAM0_RAM0On   << POWER_RAMON_ONRAM0_Pos
                     | POWER_RAMON_ONRAM1_RAM1Off  << POWER_RAMON_ONRAM1_Pos
                     | POWER_RAMON_OFFRAM0_RAM0Off << POWER_RAMON_OFFRAM0_Pos
                     | POWER_RAMON_OFFRAM1_RAM1Off << POWER_RAMON_OFFRAM1_Pos;
    
    while(1)
    {     
        // If BUTTON0 is pressed..
        if(nrf_gpio_pin_read(BSP_BUTTON_1) == BTN_PRESSED)
        {
            nrf_gpio_pin_clear(BSP_LED_1);
            
            // Start the RTC timer
            NRF_RTC1->TASKS_START = 1;
            
            // Keep entering system ON as long as Button 1 is not pressed
            do
            {
                // Enter System ON sleep mode
                __WFE();  
                // Make sure any pending events are cleared
                __SEV();
                __WFE();                
            }while(nrf_gpio_pin_read(BUTTON_1) != BTN_PRESSED);
            
            // Stop and clear the RTC timer
            NRF_RTC1->TASKS_STOP = 1;
            NRF_RTC1->TASKS_CLEAR = 1;
            
            nrf_gpio_pin_set(BSP_LED_1);
            nrf_gpio_pin_clear(BSP_LED_1);
        }
    }
}
void RTC1_IRQHandler(void)
{
	// This handler will be run after wakeup from system ON (RTC wakeup)
	if(NRF_RTC1->EVENTS_COMPARE[0])
	{
		NRF_RTC1->EVENTS_COMPARE[0] = 0;
		
		nrf_gpio_pin_toggle(LED_1);
		
		NRF_RTC1->TASKS_CLEAR = 1;
	}
}


/** @} */
