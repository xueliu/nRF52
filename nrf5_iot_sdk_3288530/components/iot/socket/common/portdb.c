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

#include "sdk_config.h"
#include "socket.h"
#include "portdb.h"
#include <string.h>
#include "iot_common.h"
#include "iot_errors.h"

#define IANA_EPHEMRAL_BEGIN 49152u
#define IANA_EPHEMRAL_END   65535u

static uint16_t portdb[SOCKET_MAX_SOCKET_COUNT];

void
portdb_init(void)
{
    memset(&portdb[0], 0, sizeof(portdb));
}

static uint32_t
portdb_find_available_index(int * p_idx, uint16_t port)
{
    uint32_t err_code = SOCKET_NO_AVAILABLE_PORTS;
    int idx = -1;

    for (int i = 0; i < SOCKET_MAX_SOCKET_COUNT; i++) {
        if (portdb[i] == port) {
            err_code = SOCKET_PORT_IN_USE;
            break;
        }
        if (portdb[i] == 0 && idx < 0) {
            idx = i;
            err_code = NRF_SUCCESS;
            break;
        }
    }
    if (idx >= 0) {
        *p_idx = idx;
    }
    return err_code;
}

uint32_t
portdb_register(uint16_t port)
{
    int idx = 0;
    uint32_t err_code = portdb_find_available_index(&idx, port);
    if (err_code == NRF_SUCCESS) {
        portdb[idx] = port;
    }
    return err_code;
}

uint32_t
portdb_alloc(uint16_t * p_port)
{
    uint32_t err_code = SOCKET_NO_AVAILABLE_PORTS;
    for (uint32_t i = IANA_EPHEMRAL_BEGIN; i <= IANA_EPHEMRAL_END; i++) {
		uint16_t port = (uint16_t)i;
        err_code = portdb_register(port);
        if (err_code == NRF_SUCCESS) {
            *p_port = port;
            break;
        } else if (err_code == SOCKET_NO_AVAILABLE_PORTS) {
            break;
        }
    }
    return err_code;
}

void
portdb_free(uint16_t port)
{
    for (int i = 0; i < SOCKET_MAX_SOCKET_COUNT; i++) {
        if (portdb[i] == port) {
            portdb[i] = 0;
            break;
        }
    }
}
