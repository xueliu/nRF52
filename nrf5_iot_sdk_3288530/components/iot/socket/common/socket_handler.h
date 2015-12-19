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

#ifndef SOCKET_HANDLER_H__
#define SOCKET_HANDLER_H__

#include "iot_defines.h"
#include "socket_api.h"
#include "mbuf.h"

typedef struct {
    socket_family_t     family;
    socket_protocol_t   protocol;
    socket_type_t       type;
} socket_params_t;

typedef struct {
    void             * p_ctx;
    int                flags;
    mbuf_head_t        mbuf_head;
    uint16_t           read_events;
    socket_params_t    params;
} socket_handle_t;

typedef uint32_t (*create_handler_t)(socket_handle_t * p_socket_handle);
typedef uint32_t (*connect_handler_t)(socket_handle_t * p_socket_handle,
                                      const void * p_addr,
                                      socklen_t addrlen);

typedef uint32_t (*send_handler_t)(socket_handle_t * p_socket_handle,
                                   const void * p_buf,
                                   uint32_t len,
                                   int flags);

typedef uint32_t (*bind_handler_t)(socket_handle_t * p_socket_handle,
                                   const void * p_myaddr,
                                   socklen_t addrlen);

typedef uint32_t (*listen_handler_t)(socket_handle_t * p_socket_handle, int buflen);

typedef uint32_t (*accept_handler_t)(socket_handle_t * p_socket_handle,
                                     void * p_cliaddr,
                                     socklen_t * p_addrlen);
typedef uint32_t (*close_handler_t)(socket_handle_t * p_socket_handle);

typedef struct {
    create_handler_t  create_handler;
    connect_handler_t connect_handler;
    send_handler_t    send_handler;
    bind_handler_t    bind_handler;
    listen_handler_t  listen_handler;
    accept_handler_t  accept_handler;
    close_handler_t   close_handler;
} socket_handler_t;

#if SOCKET_IPV6_ENABLE == 1 || SOCKET_LWIP_ENABLE == 1
extern socket_handler_t transport_handler;
void transport_handler_init(void);
#endif


#endif
