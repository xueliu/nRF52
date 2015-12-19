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
#include "nordic_common.h"
#include "sdk_common.h"
#include "sdk_config.h"
#include "iot_common.h"
#include "app_error.h"
#include "app_trace.h"
#include "app_timer.h"
#include "socket.h"
#include "socket_api.h"
#include "socket_handler.h"
#include "portdb.h"
#include "nrf_soc.h"
#include "errno.h"
#include "mbuf.h"
#include "mem_manager.h"
#include "ipv6_parse.h"
#include "netinet/in.h"

#ifndef SOCKET_ENABLE_API_PARAM_CHECK
#define SOCKET_ENABLE_API_PARAM_CHECK 0
#endif

#if SOCKET_MEDIUM_ENABLE == 1
#include "socket_medium.h"
#endif

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          SOCKET_ENABLE_API_PARAM_CHECK should be set to 0 to disable these checks.
 *
 * @{
 */
#if SOCKET_ENABLE_API_PARAM_CHECK == 1

/**@brief Macro to check is module is initialized before requesting one of the module procedures. */
#define VERIFY_MODULE_IS_INITIALIZED()                                    \
    do {                                                                  \
        if(m_initialization_state == false)                               \
        {                                                                 \
            return (SDK_ERR_MODULE_NOT_INITIALZED | IOT_SOCKET_ERR_BASE); \
        }                                                                 \
    } while (0)


/**
 * @brief Verify NULL parameters are not passed to API by application.
 */
#define NULL_PARAM_CHECK(PARAM)                            \
    do {                                                   \
        if ((PARAM) == NULL)                               \
        {                                                  \
            set_errno(EFAULT);                             \
            return -1;                                     \
        }                                                  \
    } while (0)

/**
 * @brief Verify socket id passed on the API by application is valid.
 */
#define VERIFY_SOCKET_ID(ID)                                       \
    do {                                                           \
        if (((ID) < 0) || ((ID) >= SOCKET_MAX_SOCKET_COUNT))       \
        {                                                          \
            set_errno(EBADF);                                      \
            return -1;                                             \
        }                                                          \
    } while(0)


#else

#define VERIFY_MODULE_IS_INITIALIZED()
#define NULL_PARAM_CHECK(PARAM)
#define VERIFY_SOCKET_ID(ID)
#endif

/**
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case need arises to use an alternative architecture.
 * @{
 */
#define SOCKET_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_socket_mutex)                                   /**< Lock module using mutex */
#define SOCKET_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_socket_mutex)                                 /**< Unlock module using mutex */
/** @} */

SDK_MUTEX_DEFINE(m_socket_mutex)                                                            /**< Mutex variable. Currently unused, this declaration does not occupy any space in RAM. */
static bool      m_initialization_state  = false;                                        /**< Variable to maintain module initialization state. */

typedef enum
{
    STATE_CLOSED = 0,
    STATE_OPEN,
    STATE_CONNECTED
} socket_state_t;

typedef struct
{
    volatile socket_state_t     state;
    socket_handle_t             handle;
    socket_handler_t          * handler;
} socket_entry_t;

static socket_entry_t m_socket_table[SOCKET_MAX_SOCKET_COUNT];

uint32_t
socket_init(void)
{
    if (!m_initialization_state)
    {
        app_trace_init();
        memset(m_socket_table, 0, sizeof(m_socket_table));
        uint32_t err_code = nrf_mem_init();
        APP_ERROR_CHECK(err_code);
#if SOCKET_MEDIUM_ENABLE == 1
	    err_code = socket_medium_init();
#endif
	    APP_ERROR_CHECK(err_code);
        portdb_init();
#if SOCKET_IPV6_ENABLE == 1 || SOCKET_LWIP_ENABLE == 1
        transport_handler_init();
#endif

#if SOCKET_MEDIUM_ENABLE == 1
        socket_medium_start();
#endif
        m_initialization_state = true;
    }
    return NRF_SUCCESS;
}

int
fcntl(int fd, int cmd, int flags)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(fd);

    if (cmd != F_SETFL)
    {
        set_errno(EINVAL);
        return -1;
    }
    SOCKET_MUTEX_LOCK();
    socket_entry_t * p_socket_entry = &m_socket_table[fd];
    SOCKET_MUTEX_UNLOCK();

    p_socket_entry->handle.flags = flags;
    return 0;
}

static void
socket_set_errno(uint32_t err_code)
{
    switch (err_code) {
        case UDP_INTERFACE_NOT_READY: // fallthrough
        case SOCKET_INTERFACE_NOT_READY:
            set_errno(ENETDOWN);
            break;
        case SOCKET_WOULD_BLOCK:
            set_errno(EAGAIN);
            break;
        case SOCKET_NO_ROUTE:
            set_errno(ENETUNREACH);
            break;
        case NRF_ERROR_NO_MEM: // fallthrough
        case SOCKET_NO_MEM:
            set_errno(ENOMEM);
            break;
        case SOCKET_TIMEOUT:
            set_errno(ETIMEDOUT);
            break;
        case SOCKET_NO_AVAILABLE_PORTS:
            set_errno(EMFILE);
            break;
        case SOCKET_PORT_IN_USE:
            set_errno(EADDRINUSE);
            break;
        case SOCKET_INVALID_PARAM:
            set_errno(EINVAL);
            break;
        case SOCKET_UNSUPPORTED_PROTOCOL:
            set_errno(EPROTONOSUPPORT);
            break;
    }
}

/**
 * Finds a free entry in the socket table, marks it as used and returns it. Returns -1 if no entry
 * was found.
 */
static int
socket_allocate(void)
{
    int ret_sock = -1;
    SOCKET_MUTEX_LOCK();
    for (int sock = 0; sock < SOCKET_MAX_SOCKET_COUNT; sock++)
    {
        SOCKET_TRACE("Looking at socket %d with state %d\r\n", (int)sock, m_socket_table[sock].state);
        if (m_socket_table[sock].state == STATE_CLOSED)
        {
            m_socket_table[sock].state = STATE_OPEN;
            ret_sock = sock;
            break;
        }
    }
    if (ret_sock < 0)
    {
        set_errno(EMFILE);
    }
    SOCKET_MUTEX_UNLOCK();
    return ret_sock;
}

static void
socket_free(int sock)
{
    SOCKET_TRACE("Freeing socket %d\r\n", (int)sock);
    SOCKET_MUTEX_LOCK();
    memset(&m_socket_table[sock], 0, sizeof(m_socket_table[sock]));
    m_socket_table[sock].state = STATE_CLOSED;
    SOCKET_MUTEX_UNLOCK();
}

int
socket(socket_family_t family, socket_type_t type, socket_protocol_t protocol)
{
    if (m_initialization_state == false)
    {
        (void) socket_init();
    }
    VERIFY_MODULE_IS_INITIALIZED();

    int ret_sock = -1;
    int sock = socket_allocate();
    SOCKET_TRACE("Got value %d from allocate\r\n", (int)sock);
    if (sock >= 0)
    {
        socket_entry_t *p_socket_entry = &m_socket_table[sock];
        p_socket_entry->handle.params.family = family;
        p_socket_entry->handle.params.protocol = protocol;
        p_socket_entry->handle.params.type = type;
        p_socket_entry->handler = NULL;

        if (family == AF_INET6)
        {
#if SOCKET_IPV6_ENABLE == 1 || SOCKET_LWIP_ENABLE == 1
            p_socket_entry->handler = &transport_handler;
#else
            set_errno(EAFNOSUPPORT);
#endif
        // } else if (family == AF_BLE) {
            // TODO: Handle BLE
        }
        else
        {
            set_errno(EAFNOSUPPORT);
        }

        if (p_socket_entry->handler != NULL)
        {
            uint32_t err_code = p_socket_entry->handler->create_handler(&p_socket_entry->handle);
            socket_set_errno(err_code);
            ret_sock = (err_code == NRF_SUCCESS) ? sock : ret_sock;
        }

        if (ret_sock < 0)
        {
            socket_free(sock);
        }
    }
    SOCKET_TRACE("Returning socket value %d\r\n", (int)ret_sock);
    return ret_sock;
}

int
connect(int sock, const void * p_addr, socklen_t addrlen)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);
    NULL_PARAM_CHECK(p_addr);

    SOCKET_MUTEX_LOCK();
    socket_entry_t * p_socket_entry = &m_socket_table[sock];
    SOCKET_MUTEX_UNLOCK();

    int ret = -1;
    if (p_socket_entry->state == STATE_OPEN)
    {
        uint32_t err_code = p_socket_entry->handler->connect_handler(&p_socket_entry->handle,
                                                                     p_addr,
                                                                     addrlen);
        if (err_code == NRF_SUCCESS)
        {
            p_socket_entry->state = STATE_CONNECTED;
            ret = 0;
        }
        socket_set_errno(err_code);
    }
    else if (p_socket_entry->state == STATE_CONNECTED)
    {
        set_errno(EISCONN);
    }
    else if (p_socket_entry->state == STATE_CLOSED)
    {
        set_errno(EBADF);
    }
    return ret;
}

ssize_t
send(int sock, const void * p_buf, size_t buflen, int flags)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);
    NULL_PARAM_CHECK(p_buf);

    SOCKET_MUTEX_LOCK();
    socket_entry_t * p_socket_entry = &m_socket_table[sock];
    SOCKET_MUTEX_UNLOCK();

    ssize_t ret = -1;
    if (p_socket_entry->state == STATE_CONNECTED)
    {
        uint32_t err_code = p_socket_entry->handler->send_handler(&p_socket_entry->handle,
                                                                  p_buf,
                                                                  buflen,
                                                                  flags);
        if (err_code == NRF_SUCCESS)
        {
            ret = (ssize_t) buflen;
        }
        socket_set_errno(err_code);
    }
    else
    {
        set_errno(ENOTCONN);
    }
    return ret;
}

ssize_t
write(int sock, const void * p_buf, size_t buflen)
{
    return send(sock, p_buf, buflen, 0);
}

static uint32_t
socket_recv(socket_handle_t * p_socket_handle,
            void * p_buf,
            uint32_t buf_size,
            uint32_t * buf_len,
            int flags)
{
    if ((p_socket_handle->flags & O_NONBLOCK) != 0 &&
        (flags & MSG_WAITALL) == 0)
    {
        flags |= MSG_DONTWAIT;
    }
    uint32_t err_code = NRF_SUCCESS;
    if (mbuf_empty(&p_socket_handle->mbuf_head) == true)
    {
        if ((flags & MSG_DONTWAIT) != 0)
        {
            err_code = SOCKET_WOULD_BLOCK;
        }
        else
        {
            while ((mbuf_empty(&p_socket_handle->mbuf_head) == true) && (err_code == NRF_SUCCESS))
            {
                err_code = sd_app_evt_wait();
            }
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        *buf_len = mbuf_read(&p_socket_handle->mbuf_head, p_buf, buf_size);
        p_socket_handle->read_events = 0;
    }
    return err_code;
}

ssize_t recv(int sock, void * p_buf, size_t buf_size, int flags)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);
    NULL_PARAM_CHECK(p_buf);

    SOCKET_MUTEX_LOCK();
    socket_entry_t * p_socket_entry = &m_socket_table[sock];
    SOCKET_MUTEX_UNLOCK();

    ssize_t ret = -1;
    if (p_socket_entry->state == STATE_CONNECTED)
    {
        uint32_t recv_size = 0;
        uint32_t err_code = socket_recv(&p_socket_entry->handle,
                                        p_buf,
                                        buf_size,
                                        &recv_size,
                                        flags);
        if (err_code == NRF_SUCCESS)
        {
            ret = (ssize_t) recv_size;
        }
        socket_set_errno(err_code);
    }
    else
    {
        set_errno(ENOTCONN);
    }
    return ret;
}

ssize_t read(int sock, void * p_buf, size_t buf_size)
{
    return recv(sock, p_buf,  buf_size, 0);
}

int setsockopt(int sock, socket_opt_lvl_t level, int optname, const void *optval, socklen_t optlen)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);

    (void) sock;
    (void) level;
    (void) optname;
    (void) optval;
    (void) optlen;

    return -1;
}

int getsockopt(int sock, socket_opt_lvl_t level, int optname, void *optval, socklen_t *optlen)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);

    (void) sock;
    (void) level;
    (void) optname;
    (void) optval;
    (void) optlen;

    return -1;
}

int bind(int sock, const void * p_myaddr, socklen_t addrlen)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);
    NULL_PARAM_CHECK(p_myaddr);

    (void) sock;
    (void) p_myaddr;
    (void) addrlen;

    return -1;
}

int listen(int sock, int backlog)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);

    (void) sock;
    (void) backlog;

    return -1;
}

int accept(int sock, void * p_cliaddr, socklen_t * p_addrlen)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);
    NULL_PARAM_CHECK(p_cliaddr);

    (void) sock;
    (void) p_cliaddr;
    (void) p_addrlen;

    return -1;
}

int
close(int sock)
{
    VERIFY_MODULE_IS_INITIALIZED();
    VERIFY_SOCKET_ID(sock);

    SOCKET_MUTEX_LOCK();
    socket_entry_t * p_socket_entry = &m_socket_table[sock];
    SOCKET_MUTEX_UNLOCK();

    uint32_t err_code = p_socket_entry->handler->close_handler(&p_socket_entry->handle);
    int ret = (err_code == NRF_SUCCESS) ? 0 : -1;
    SOCKET_TRACE("Close socket %d: ret: %d\r\n", (int)sock, ret);
    socket_free(sock);
    return ret;
}

int
fd_set_cmp(fd_set * set_a, fd_set * set_b)
{
    for (uint32_t i = 0; i < FD_SETSIZE; i++)
    {
        if (FD_ISSET(i, set_a) != FD_ISSET(i, set_b))
        {
            return 1;
        }
    }
    return 0;
}

int select(int nfds,
           fd_set * p_readset,
           fd_set * p_writeset,
           fd_set * p_exceptset,
           const struct timeval * timeout)
{
    VERIFY_SOCKET_ID(nfds - 1);
    // TODO: Support writeset and exceptset
    (void) p_writeset;
    (void) p_exceptset;

    if (timeout != NULL)
    {
        set_errno(EINVAL);
        return -1;
    }

    fd_set readset;
    FD_ZERO(&readset);

    int num_ready = 0;
    uint32_t err_code = NRF_SUCCESS;
    while (err_code == NRF_SUCCESS)
    {
        SOCKET_MUTEX_LOCK();
        for (int sock = 0; sock < nfds; sock++)
        {
            socket_entry_t * p_socket_entry = &m_socket_table[sock];
            if (FD_ISSET(sock, p_readset) &&
                p_socket_entry->handle.read_events > 0)
            {
                FD_SET(sock, &readset);
                num_ready++;
            }
            else
            {
                FD_CLR(sock, &readset);
            }
        }
        SOCKET_MUTEX_UNLOCK();
        // TODO: Check out how app events queue up while we checked the socket
        if (fd_set_cmp(p_readset, &readset) == 0)
        {
            break;
        }
        else
        {
            err_code = sd_app_evt_wait();
        }

    }

    return num_ready;
}

int inet_pton(socket_family_t af, const char * src, void * dst)
{
    int ret_val = 1;
    if (af != AF_INET6)
    {
        ret_val = -1;
    }
    else
    {
        in6_addr_t * p_addr = (in6_addr_t *)dst;
        uint32_t err_code = ipv6_parse_addr(p_addr->s6_addr, src, strlen(src));
        if (err_code != NRF_SUCCESS)
        {
            ret_val = 0;
        }
    }
    return ret_val;
}
