/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
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

#ifndef PORTDB_H__
#define PORTDB_H__

#include <stdint.h>
#include "iot_defines.h"
#include "sdk_common.h"

void portdb_init(void);
uint32_t portdb_alloc(uint16_t * p_port);
uint32_t portdb_register(uint16_t port);
void portdb_free(uint16_t port_number);

#endif
