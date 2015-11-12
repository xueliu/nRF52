/*
 * File      : board.h
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 */

#ifndef __BOARD_H__
#define __BOARD_H__

// <o> Internal SRAM memory size[Kbytes] <64>
//#ifdef __ICCARM__
//// Use *.icf ram symbal, to avoid hardcode.
//extern char __ICFEDIT_region_RAM_end__;
//#define NRF_SRAM_END          &__ICFEDIT_region_RAM_end__
//#else
#define NRF_SRAM_SIZE         60
#define NRF_SRAM_END          (0x20000000 + NRF_SRAM_SIZE * 1024)
//#endif

void rt_hw_board_init(void);

#endif
