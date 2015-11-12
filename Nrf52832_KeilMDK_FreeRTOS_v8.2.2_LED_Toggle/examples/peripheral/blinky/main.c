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
 *
 * @defgroup blinky_example_main main.c
 * @{
 * @ingroup blinky_example
 * @brief Blinky Example Application main file.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"

const uint8_t leds_list[LEDS_NUMBER] = LEDS_LIST;


/* FreeRTOS Includes ---------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"


/** @addtogroup Template
  * @{
  */ 

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Task priorities. */

#define mainCREATOR_TASK_PRIORITY         ( tskIDLE_PRIORITY + 3 )

#define Task1STACK_SIZE		configMINIMAL_STACK_SIZE   
#define Task2STACK_SIZE		configMINIMAL_STACK_SIZE   
#define Task3STACK_SIZE		configMINIMAL_STACK_SIZE   
#define Task4STACK_SIZE		configMINIMAL_STACK_SIZE

void vAltStartTask1Tasks( unsigned portBASE_TYPE uxPriority);          
void vAltStartTask2Tasks( unsigned portBASE_TYPE uxPriority);       
void vAltStartTask3Tasks( unsigned portBASE_TYPE uxPriority);       
void vAltStartTask4Tasks( unsigned portBASE_TYPE uxPriority);   

static  portTASK_FUNCTION_PROTO( vTask1FunctionTask, pvParameters );    
static  portTASK_FUNCTION_PROTO( vTask2FunctionTask, pvParameters );      
static  portTASK_FUNCTION_PROTO( vTask3FunctionTask, pvParameters );  
static  portTASK_FUNCTION_PROTO( vTask4FunctionTask, pvParameters );

uint64_t  Task1Task_Counter = 0;                                  
uint64_t  Task2Task_Counter = 0;                                    
uint64_t  Task3Task_Counter = 0;                                    
uint64_t  Task4Task_Counter = 0;

static void prvSetupHardware( void );


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    /* Steup LEDs Gpio. */
    prvSetupHardware();
    
    /* Create task. */
    vAltStartTask1Tasks( mainCREATOR_TASK_PRIORITY);    //Task1
    vAltStartTask2Tasks( mainCREATOR_TASK_PRIORITY);    //Task2
    vAltStartTask3Tasks( mainCREATOR_TASK_PRIORITY);    //Task3
		vAltStartTask4Tasks( mainCREATOR_TASK_PRIORITY);    //Task4
    
    /* Start the scheduler. */
    vTaskStartScheduler();
	
    /* Will only get here if there was not enough heap space to create the idle task. */
    return 0;
}

static void prvSetupHardware( void )
{
    // Configure LED-pins as outputs.
    LEDS_CONFIGURE(LEDS_MASK);
}

/*-----------------------------------------------------------*/      
void vAltStartTask1Tasks( unsigned portBASE_TYPE uxPriority)           
{ 
        xTaskCreate( vTask1FunctionTask, "Task1", Task1STACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );  //Task1
}

void vAltStartTask2Tasks( unsigned portBASE_TYPE uxPriority)        
{ 
        xTaskCreate( vTask2FunctionTask, "Task2", Task2STACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );  //Task2
}

void vAltStartTask3Tasks( unsigned portBASE_TYPE uxPriority)	
{ 
				xTaskCreate( vTask3FunctionTask, "Task3", Task3STACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );  //Task3
}

void vAltStartTask4Tasks( unsigned portBASE_TYPE uxPriority)	
{ 
				xTaskCreate( vTask4FunctionTask, "Task4", Task3STACK_SIZE, NULL, uxPriority, ( xTaskHandle * ) NULL );  //Task4
}
/*-----------------------------------------------------------*/  
//Task1
static portTASK_FUNCTION( vTask1FunctionTask, pvParameters )           
{
 
        portTickType xLastWakeTime;
          
	/* Just to stop compiler warnings. */
	( void ) pvParameters;
        xLastWakeTime = xTaskGetTickCount();

	for( ;; )
	{
          Task1Task_Counter++;
          LEDS_INVERT(1 << LED_1);
          vTaskDelayUntil( &xLastWakeTime, ( 100 / portTICK_RATE_MS ) );
	}
}
//Task2
static portTASK_FUNCTION( vTask2FunctionTask, pvParameters )             
{        
        portTickType xLastWakeTime;
        
	/* Just to stop compiler warnings. */
	( void ) pvParameters;
        
        xLastWakeTime = xTaskGetTickCount();
	for( ;; )
	{
          Task2Task_Counter++;
          LEDS_INVERT(1 << LED_2);
          vTaskDelayUntil( &xLastWakeTime, ( 200 / portTICK_RATE_MS ) );
	}
}
//Task3
static portTASK_FUNCTION( vTask3FunctionTask, pvParameters )             
{       
        portTickType xLastWakeTime;
        
	/* Just to stop compiler warnings. */
	( void ) pvParameters;
        
        xLastWakeTime = xTaskGetTickCount();
	for( ;; )
	{
          Task3Task_Counter++;   
          LEDS_INVERT(1 << LED_3);
          vTaskDelayUntil( &xLastWakeTime, ( 500 / portTICK_RATE_MS ) );
	}
}
//Task4
static portTASK_FUNCTION( vTask4FunctionTask, pvParameters )             
{       
        portTickType xLastWakeTime;
        
	/* Just to stop compiler warnings. */
	( void ) pvParameters;
        
        xLastWakeTime = xTaskGetTickCount();
	for( ;; )
	{
          Task4Task_Counter++;   
          LEDS_INVERT(1 << LED_4);
          vTaskDelayUntil( &xLastWakeTime, ( 1000 / portTICK_RATE_MS ) );
	}
}


/** @} */
