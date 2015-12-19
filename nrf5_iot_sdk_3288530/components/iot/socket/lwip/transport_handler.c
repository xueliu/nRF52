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

#include "iot_defines.h"
#include "iot_errors.h"
#include "sdk_config.h"
#include "socket_common.h"
#include "socket_handler.h"
#include "portdb.h"
#include "nrf_soc.h"
#include "app_timer.h"
#include "mem_manager.h"

#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/timers.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"

#define VERIFY_ADDRESS_LEN(len)                          \
    do {                                                 \
        if ((len) != sizeof(sockaddr_in6_t)) {           \
            return NRF_ERROR_NULL | IOT_SOCKET_ERR_BASE; \
        }                                                \
    } while (0)

#define APP_TIMER_PRESCALER                 NRF_DRIVER_TIMER_PRESCALER
#define RECV_BUFFER_MAX_SIZE                1024u
#define LWIP_SYS_TIMER_INTERVAL             APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)               /**< Interval for timer used as trigger to send. */

typedef enum
{
    TCP_STATE_IDLE,
    TCP_STATE_REQUEST_CONNECTION,
    TCP_STATE_CONNECTED,
    TCP_STATE_DATA_TX_IN_PROGRESS,
    TCP_STATE_TCP_SEND_PENDING,
    TCP_STATE_DISCONNECTED
} tcp_state_t;

typedef struct {
    struct tcp_pcb  * p_pcb;
    socket_handle_t * p_socket_handle;
    volatile tcp_state_t       tcp_state;
} lwip_handle_t;

static lwip_handle_t lwip_handles[SOCKET_MAX_SOCKET_COUNT];

APP_TIMER_DEF(m_lwip_timer_id);

static err_t lwip_recv_callback(void * p_arg, struct tcp_pcb * p_pcb, struct pbuf * p_pbuf, err_t err);

static uint8_t *
mbuf_buf(mbuf_t * p_mbuf)
{
    struct pbuf * p_pbuf = (struct pbuf *)p_mbuf->p_ctx;
    return p_pbuf->payload;
}

static uint32_t
mbuf_buf_len(mbuf_t * p_mbuf)
{
    struct pbuf * p_pbuf = (struct pbuf *)p_mbuf->p_ctx;
    return p_pbuf->len;
}

static void
mbuf_freefn(void * p_ctx)
{
    (void) pbuf_free((struct pbuf *)p_ctx);
}

static void
lwip_timer_callback(void * p_ctx)
{
    (void) p_ctx;
    sys_check_timeouts();
}

static volatile bool m_is_interface_up;

void nrf_driver_interface_up(void)
{
    m_is_interface_up = true;
}

void nrf_driver_interface_down(void)
{
    m_is_interface_up = false;
    // TODO: Disconnect all sockets
}


void
transport_handler_init(void)
{
    m_is_interface_up = false;
    lwip_init();
    uint32_t err_code = nrf_driver_init();
    APP_ERROR_CHECK(err_code);
    for (uint32_t i = 0; i< SOCKET_MAX_SOCKET_COUNT; i++)
    {
        lwip_handles[i].p_pcb = NULL;
        lwip_handles[i].p_socket_handle = NULL;
        lwip_handles[i].tcp_state = TCP_STATE_IDLE;
    }
    err_code = app_timer_create(&m_lwip_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                lwip_timer_callback);
    APP_ERROR_CHECK(err_code);
    err_code = app_timer_start(m_lwip_timer_id, LWIP_SYS_TIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
    SOCKET_TRACE("Initialized LWIP transport handler\r\n");
}

static lwip_handle_t * lwip_handle_allocate(socket_handle_t * p_socket_handle)
{
    lwip_handle_t * p_handle = NULL;
    for (uint32_t i = 0; i < SOCKET_MAX_SOCKET_COUNT; i++)
    {
        if (lwip_handles[i].p_pcb == NULL)
        {
            p_handle = &lwip_handles[i];
            mbuf_init(&p_socket_handle->mbuf_head, mbuf_buf, mbuf_buf_len, mbuf_freefn);
            p_handle->p_pcb = tcp_new_ip6();
            p_handle->p_socket_handle = p_socket_handle;
            p_handle->tcp_state = TCP_STATE_IDLE;
            tcp_arg(p_handle->p_pcb, p_handle);
            tcp_setprio(p_handle->p_pcb, TCP_PRIO_MIN);
            tcp_recv(p_handle->p_pcb, lwip_recv_callback);
            break;
        }
    }
    SOCKET_TRACE("Allocated LWIP socket handle\r\n");
    return p_handle;
}

static void lwip_handle_free(lwip_handle_t * p_handle)
{
    p_handle->p_pcb = NULL;
    p_handle->p_socket_handle = NULL;
    p_handle->tcp_state = TCP_STATE_IDLE;
    SOCKET_TRACE("Released LWIP socket handle\r\n");
}

static uint32_t
lwip_transport_create(socket_handle_t * p_socket_handle)
{
    lwip_handle_t * p_handle = NULL;
    uint32_t err_code = NRF_SUCCESS;
    switch (p_socket_handle->params.type)
    {
        case SOCK_STREAM:
            p_handle = lwip_handle_allocate(p_socket_handle);
            if (p_handle == NULL)
            {
                err_code = NRF_ERROR_NULL;
            }
            else
            {
                p_socket_handle->p_ctx = p_handle;
            }
            break;
        default:
            err_code = NRF_ERROR_NULL;
            break;
    }
    return err_code;
}

uint32_t lwip_error_convert(err_t lwip_err)
{
    uint32_t err_code = NRF_ERROR_NULL;
    switch (lwip_err)
    {
        case ERR_OK:
            err_code = NRF_SUCCESS;
            break;
        case ERR_MEM:
            err_code = SOCKET_NO_MEM;
            break;
        case ERR_TIMEOUT:
            err_code = SOCKET_TIMEOUT;
            break;
        case ERR_RTE:
            err_code = SOCKET_NO_ROUTE;
            break;
        default:
            err_code = NRF_ERROR_NULL;
            break;
    }
    return err_code;
}

static uint32_t
lwip_transport_close(socket_handle_t * p_socket_handle)
{
    lwip_handle_t * p_handle = (lwip_handle_t *)p_socket_handle->p_ctx;
    portdb_free((uint16_t)p_handle->p_pcb->local_port);
    err_t err_code = tcp_close(p_handle->p_pcb);
    if (err_code == ERR_OK)
    {
        lwip_handle_free(p_handle);
    }
    return lwip_error_convert(err_code);
}

static err_t
lwip_connect_callback(void * p_arg, struct tcp_pcb * p_pcb, err_t err)
{
    lwip_handle_t * p_handle = (lwip_handle_t *)p_arg;
    // TODO: Error check
    SOCKET_TRACE("Got connection callback with err %d\r\n", (int)err);
    p_handle->tcp_state = TCP_STATE_CONNECTED;
    return ERR_OK;
}

static err_t
lwip_recv_callback(void * p_arg, struct tcp_pcb * p_pcb, struct pbuf * p_pbuf, err_t err)
{
    lwip_handle_t * p_handle = (lwip_handle_t *)p_arg;
    socket_handle_t * p_socket_handle = p_handle->p_socket_handle;
    if (err == ERR_OK)
    {
        if (mbuf_size_total(&p_socket_handle->mbuf_head) >= (RECV_BUFFER_MAX_SIZE - p_pbuf->len))
        {
            err = ERR_MEM;
        }
        else
        {
            mbuf_t * p_mbuf;
            uint32_t err_code = mbuf_alloc(&p_mbuf, p_pbuf);
            if (err_code == NRF_SUCCESS)
            {
                mbuf_write(&p_socket_handle->mbuf_head, p_mbuf);
                tcp_recved(p_handle->p_pcb, p_pbuf->tot_len);
                p_socket_handle->read_events++;
            }
            else
            {
                err = ERR_MEM;
            }
        }
    }
    return err;
}

static uint32_t
lwip_wait_for_state(lwip_handle_t * p_handle, tcp_state_t state)
{
    uint32_t err_code = NRF_SUCCESS;
    while (err_code == NRF_SUCCESS &&
           p_handle->tcp_state != state &&
           p_handle->tcp_state != TCP_STATE_DISCONNECTED)
    {
         err_code = sd_app_evt_wait();
    }
    if (err_code == NRF_SUCCESS && p_handle->tcp_state != state)
    {
        err_code = NRF_ERROR_NULL;
    }
    return err_code;
}

static uint32_t
lwip_transport_connect(socket_handle_t * p_socket_handle, const void * p_addr, socklen_t p_addr_len)
{
    VERIFY_ADDRESS_LEN(p_addr_len);

    uint32_t err_code = NRF_SUCCESS;
    bool is_blocking = ((p_socket_handle->flags & O_NONBLOCK) == 0);
    lwip_handle_t * p_handle = (lwip_handle_t *)p_socket_handle->p_ctx;
    const sockaddr_in6_t * p_addr_in6 = (const sockaddr_in6_t *)p_addr;
    uint16_t port = 0;

    if (m_is_interface_up == false)
    {
        if (is_blocking)
        {
            while (err_code == NRF_SUCCESS && m_is_interface_up == false)
            {
                err_code = sd_app_evt_wait();
            }
        }
        else
        {
            err_code = SOCKET_INTERFACE_NOT_READY;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = portdb_alloc(&port);
        SOCKET_TRACE("Err %d from portdb\r\n", (int)err_code);
    }

    SOCKET_TRACE("Binding to port %d\r\n", (int)port);
    if (err_code == NRF_SUCCESS)
    {
        ip6_addr_t any_addr;
        ip6_addr_set_any(&any_addr);
        err_t err = tcp_bind_ip6(p_handle->p_pcb, &any_addr, port);
        SOCKET_TRACE("Err %d from bind\r\n", (int)err);
        err_code = lwip_error_convert(err);
    }

    if (err_code == NRF_SUCCESS)
    {

        ip6_addr_t remote_addr;
        const struct in6_addr * addr = &p_addr_in6->sin6_addr;
        IP6_ADDR(&remote_addr, 0, addr->s6_addr[0], addr->s6_addr[1], addr->s6_addr[2], addr->s6_addr[3]);
        IP6_ADDR(&remote_addr, 1, addr->s6_addr[4], addr->s6_addr[5], addr->s6_addr[6], addr->s6_addr[7]);
        IP6_ADDR(&remote_addr, 2, addr->s6_addr[8], addr->s6_addr[9], addr->s6_addr[10], addr->s6_addr[11]);
        IP6_ADDR(&remote_addr, 3, addr->s6_addr[12], addr->s6_addr[13], addr->s6_addr[14], addr->s6_addr[15]);

        p_handle->tcp_state = TCP_STATE_REQUEST_CONNECTION;
        err_t err = tcp_connect_ip6(p_handle->p_pcb, &remote_addr, HTONS(p_addr_in6->sin6_port), lwip_connect_callback);
        SOCKET_TRACE("Err %d from connect\r\n", (int)err);
        err_code = lwip_error_convert(err);

        if (err_code == NRF_SUCCESS && is_blocking)
        {
            err_code = lwip_wait_for_state(p_handle, TCP_STATE_CONNECTED);
        }
    }
    if (err_code != NRF_SUCCESS)
    {
        SOCKET_TRACE("Error %d when connecting to socket\r\n", (int)err_code);
    }
    else
    {
        SOCKET_TRACE("Successfully connected to remote host!\r\n");
    }

    return err_code;
}

static err_t
lwip_send_complete(void * p_arg, struct tcp_pcb * p_pcb, u16_t len)
{
    lwip_handle_t * p_handle = (lwip_handle_t *)p_arg;
    if (p_handle->tcp_state == TCP_STATE_TCP_SEND_PENDING)
    {
        p_handle->tcp_state = TCP_STATE_DATA_TX_IN_PROGRESS;
    }
    return ERR_OK;
}

static uint32_t
lwip_transport_send(socket_handle_t * p_socket_handle,
                    const void *p_buf,
                    uint32_t buf_len,
                    int flags)
{
    lwip_handle_t * p_handle = (lwip_handle_t *)p_socket_handle->p_ctx;
    if ((p_socket_handle->flags & O_NONBLOCK) != 0 &&
        (flags & MSG_WAITALL) == 0)
    {
        flags |= MSG_DONTWAIT;
    }

    uint32_t err_code = NRF_SUCCESS;
    uint32_t len = tcp_sndbuf(p_handle->p_pcb);
    if (len >= buf_len)
    {
        tcp_sent(p_handle->p_pcb, lwip_send_complete);
        p_handle->tcp_state = TCP_STATE_TCP_SEND_PENDING;
        err_t err = tcp_write(p_handle->p_pcb, p_buf, buf_len, 1);
        err_code = lwip_error_convert(err);
        if (err_code == NRF_SUCCESS &&
           (flags & MSG_DONTWAIT) == 0)
        {
            err_code = lwip_wait_for_state(p_handle, TCP_STATE_DATA_TX_IN_PROGRESS);
        }
    }
    else
    {
        err_code = SOCKET_NO_MEM;
    }
    return err_code;
}

socket_handler_t transport_handler = {
    .create_handler  = lwip_transport_create,
    .connect_handler = lwip_transport_connect,
    .send_handler    = lwip_transport_send,
    .close_handler   = lwip_transport_close
};
