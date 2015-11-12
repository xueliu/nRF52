/*
 * File      : application.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2015, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2015-03-01     Yangfs       the first version
 * 2015-03-27     Bernard      code cleanup.
 */

/**
 * @addtogroup NRF52832
 */
/*@{*/

#include <rtthread.h>

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <shell.h>
#endif

#define THREAD_PRIORITY 	25
#define THREAD_STACK_SIZE 	512
#define THREAD_TIMESLICE 	5

/* points to thread structure */
static rt_thread_t tid1 = RT_NULL;
static rt_thread_t tid2 = RT_NULL;

/* thread entry */
static void thread_entry(void * parameter)
{
	rt_uint32_t count = 0;
	rt_uint32_t no = (rt_uint32_t) parameter; /* get parameter of thread entry */
	
	while (1)
	{
		/* 打印线程计数值输出*/
		rt_kprintf("thread%d count: %d\n", no, count ++);
		/* 休眠10个OS Tick */
		rt_thread_delay(100);
	}
}

/* application entry */
int rt_application_init(void)
{
	/* create thread 1 */
	tid1 = rt_thread_create("t1",
		thread_entry, (void *)1,
		THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
	if(tid1 != RT_NULL)
		rt_thread_startup(tid1);
	else
		return -1;
	
	/* create thread 2 */
	tid2 = rt_thread_create("t2",
		thread_entry, (void *)2,
		THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
	if(tid2 != RT_NULL)
		rt_thread_startup(tid2);
	else
		return -1;
    /* Set finsh device */
#ifdef RT_USING_FINSH
    /* initialize finsh */
    finsh_system_init();
#endif
    return 0;
}


/*@}*/
