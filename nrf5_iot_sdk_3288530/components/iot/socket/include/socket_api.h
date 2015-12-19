/* Copyright (c)  2015 Nordic Semiconductor. All Rights Reserved.
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
 
 /** @file socket_api.h
 *
 * @defgroup iot_socket BSD Socket interface
 * @ingroup iot_sdk_socket
 * @{
 * @brief Nordic socket interface for IoT
 *
 * @details This module provides the socket interface for writing IoT applications. The API is
 *          designed to be compatible with the POSIX/BSD socket interface for the purpose of
 *          making porting easy. The socket options API has been extended to support configuring
 *          Nordic BLE stack, tuning of RF parameters as well as security options.
 */

#ifndef SOCKET_API_H__
#define SOCKET_API_H__

#include <stdint.h>

#include "sdk_common.h"
#include "iot_defines.h"
#include "errno.h"

typedef uint32_t socklen_t;

#if !defined(__GNUC__) || (__GNUC__ == 0)
typedef int32_t ssize_t;
#else
#include <sys/types.h>
#endif

#ifndef htons
#define htons(x) HTONS(x)
#endif

/**
 * @brief API for filedescriptor set.
 *
 * @details File descriptor sets are used as input to the select() function for doing I/O
 *          multiplexing. The max number of descriptors contained in a set is defined by
 *          FD_SETSIZE.
 */
#ifndef FD_ZERO

typedef uint32_t fd_set;
#define FD_ZERO(set)            (*(set) = 0)                   /**< Clear entire set.                 */
#define FD_SET(fd, set)         (*(set) |= (1u << (fd)))       /**< Set a bit in the set.             */
#define FD_CLR(fd, set)         (*(set) &= ~(1u << (fd)))      /**< Clear a bit in the set.           */
#define FD_ISSET(fd, set)       (*(set) & (1u << (fd)))        /**< Check if a bit in the set is set. */
#define FD_SETSIZE              sizeof(fd_set)                 /**< The max size of a set.            */

#endif

typedef uint16_t in_port_t;
struct timeval;

/**
 * @brief Socket families.
 */
typedef int socket_family_t;
typedef socket_family_t sa_family_t;

#define AF_INET6        2   /**< IPv6 socket family.                 */
#define AF_BLUETOOTH    3   /**< Bluetooth Low Energy socket family. */

/**
 * @brief Socket types.
 */
typedef int socket_type_t;
#define SOCK_STREAM     1        /**< TCP socket type. */
#define SOCK_DGRAM      2        /**< UDP socket type. */

/**
 * @brief Socket protocols.
 *
 * @details Using 0 should be sufficient for most users. Other
 *          values are only provided for socket API compatibility.
 */
typedef int socket_protocol_t;
#define IPPROTO_TCP      1       /**< Use TCP as transport protocol. */
#define IPPROTO_UDP      2       /**< Use UDP as transport protocol. */

/**
 * @brief Socket send/recv flags.
 */
#define MSG_DONTROUTE   0x01
#define MSG_DONTWAIT    0x02
#define MSG_OOB         0x04
#define MSG_PEEK        0x08
#define MSG_WAITALL     0x10

/**
 * @brief Generic socket address.
 *
 * @details Only provided for API compatibility.
 */
struct sockaddr {
    uint8_t         sa_len;
    socket_family_t sa_family;
    char            sa_data[14];
};

/**
 * @brief IPv6 address.
 */
struct in6_addr {
    uint8_t s6_addr[16];
};

/**
 * @brief Address record for IPv6 addresses.
 *
 * @details Contains the address and port of the host, as well as other socket options. All fields
 *          in this structure are compatible with the POSIX variant for API compatibility.
 */
struct sockaddr_in6 {
    uint8_t         sin6_len;               /**< Length of this data structure. */
    sa_family_t     sin6_family;            /**< Socket family. */
    in_port_t       sin6_port;              /**< Port, in network byte order. */

    uint32_t        sin6_flowinfo;          /**< IPv6 flow info parameters. Not used. */
    struct in6_addr sin6_addr;              /**< IPv6 address. */
    uint32_t        sin6_scope_id;          /**< IPv6 scope id. Not used. */
};

typedef struct sockaddr     sockaddr_t;
typedef struct sockaddr_in6 sockaddr_in6_t;
typedef struct in6_addr     in6_addr_t;

/**
 * @brief Socket option levels.
 */
typedef enum {
    SOL_SOCKET = 1,                         /**< Standard socket options. */
    NRF_BLE    = 2,                         /**< Nordic BLE socket options. Use this to control BLE paramters. */
    NRF_CRYPTO = 3,                         /**< Nordic crypto socket options. Use this to enable TLS/DTLS. */
    NRF_RF     = 4                          /**< Nordic radio socket options. Use this to control radio behavior. */
} socket_opt_lvl_t;


/**
 * @brief Create a socket.
 *
 * @details API to create a socket that can be used for network communication independently
 *          of lower protocol layers.
 *
 * @param[in] family    The protocol family of the network protocol to use. Currently only
 *                      AF_INET6 is supported.
 * @param[in] type      The protocol type to use for this socket.
 * @param[in] protocol  The transport protocol to use for this socket.
 *
 * @retval A non-negative socket descriptor if successful if OK, or -1 on error.
 */
int socket(socket_family_t family, socket_type_t type, socket_protocol_t protocol);

/**
 * @brief Close a socket and free any resources held by it.
 *
 * @details If the socket is already closed, this function is a noop.
 *
 * @param[in] sock  The socket to close.
 *
 * @retval 0 on success, or -1 on error.
 */
int close(int sock);


#define F_SETFL     1
#define O_NONBLOCK  0x01
/**
 *
 * @brief Control file descriptor options.
 *
 * @details Set or get file descriptor options or flags. Only supports command F_SETFL and arg O_NONBLOCK.
 *
 * @param[in] fd    The descriptor to set options on.
 * @param[in] cmd   The command class for options.
 * @param[in] flags The flags to set.
 */
int fcntl(int fd, int cmd, int flags);

/**
 * @brief Connect to an endpoint with a given address.
 *
 * @details The socket handle must be a valid handle that has not already been connected. Running 
 *          connect on an connected handle will return error.
 *
 * @param[in] sock          The socket to use for connection.
 * @param[in] p_servaddr    The address of the server to connect to. Currently, only sockaddr_in6 is
 *                          the only supported type.
 * @param[in] addrlen       The size of the p_servaddr argument.
 *
 * @retval 0 on success, or -1 on error.
 */
int connect(int sock, const void * p_servaddr, socklen_t addrlen);

/**
 * @brief Send data through a socket.
 *
 * @details By default, this function will block, UNLESS O_NONBLOCK
 *          socket option has been set, OR MSG_DONTWAIT is passed as a flag. In that case, the
 *          method will return immediately.
 *
 * @param[in] sock     The socket to write data to.
 * @param[in] p_buff   Buffer containing data to send.
 * @param[in] nbytes   Size of data contained on p_buff.
 * @param[in] flags    Flags to control send behavior.
 *
 * @retval The amount of bytes that was sent on success, or -1 on error.
 */
ssize_t send(int sock, const void * p_buff, size_t nbytes, int flags);

/**
 * @brief Write data to a socket. See \ref send() for details.
 */
ssize_t write(int sock, const void * p_buff, size_t nbytes);

/**
 * @brief Receive data on a socket.
 *
 * @details API for receiving data from a socket. By default, this function will block, UNLESS
 *          O_NONBLOCK socket option has been set, or MSG_DONTWAIT is passed as a flag.
 *
 * @param[in]  sock     The socket to receive data from.
 * @param[out] p_buff   Buffer to hold data to be read.
 * @param[in]  nbytes   Number of bytes to read. Should NOT BE larger than the size of p_buff.
 * @param[in]  flags Flags to control send behavior.
 *
 * @retval The amount of bytes that was read, or -1 on error.
 */
ssize_t recv(int sock, void * p_buff, size_t nbytes, int flags);

/**
 * @brief Read data from a socket. See \ref recv() for details.
 */
ssize_t read(int sock, void * p_buff, size_t nbytes);

/**
 * @brief Wait for read, write or exception events on a socket.
 *
 * @details Wait for a set of socket descriptors to be ready for reading, writing or having
 *          exceptions. The set of socket descriptors are configured before calling this function.
 *          This function will block until any of the descriptors in the set has any of the required
 *          events. This function is mostly useful when using O_NONBLOCK or MSG_DONTWAIT options
 *          to enable async operation.
 *
 * @param[in]    nfds          The highest socket descriptor value contained in the sets.
 * @param[inout] p_readset     The set of descriptors for which to wait for read events. Set to NULL
 *                             if not used.
 * @param[inout] p_writeset    The set of descriptors for which to wait for write events. Set to NULL
 *                             if not used.
 * @param[inout] p_exceptset   The set of descriptors for which to wait for exception events. Set to
 *                             NULL if not used.
 * @param[in]    timeout       The timeout to use for select call. Set to NULL if waiting forever.
 *
 * @retval The number of ready descriptors contained in the descriptor sets, or -1 on error.
 */
int select(int nfds,
           fd_set * p_readset,
           fd_set * p_writeset,
           fd_set * p_exceptset,
           const struct timeval * timeout);

/**
 * @brief Set socket options for a given socket.
 *
 * @details The options are grouped by level, and the option value should be the expected for the 
 *          given option, and the lifetime must be longer than that of the socket.
 *
 * @param[in] sock      The socket for which to set option.
 * @param[in] level     The level, or group to which the option belongs.
 * @param[in] optname   The name of the socket option.
 * @param[in] optval    The value to be stored for this option.
 * @param[in] optlen    The size of optval.
 *
 * @retval 0 on success, or -1 on error.
 */
int setsockopt(int sock, socket_opt_lvl_t level, int optname, const void *optval, socklen_t optlen);

/**
 * @brief Get socket options for a given socket.
 *
 * @details The options are grouped by level, and the option value is the value described by the
 *          option name.
 *
 * @param[in]       sock      The socket for which to set option.
 * @param[in]       level     The level, or group to which the option belongs.
 * @param[in]       optname   The name of the socket option.
 * @param[out]      optval    Pointer to the storage for the option value.
 * @param[inout]    optlen    The size of optval. Can be modified to the actual size of optval.
 *
 * @retval 0 on success, or -1 on error.
 */
int getsockopt(int sock, socket_opt_lvl_t level, int optname, void *optval, socklen_t *optlen);

/**
 * @brief Bind a socket to an address and prot.
 *
 * @details The provided address must be supported by the socket protocol family.
 *
 * @param[in] sock      The socket descriptor to bind.
 * @param[in] p_myaddr  The address to bind this socket to.
 * @param[in] addrlen   The size of p_myaddr.
 *
 * @retval 0 on success, or -1 on error.
 */
int bind(int sock, const void * p_myaddr, socklen_t addrlen);

/**
 * @brief Mark a socket as listenable.
 *
 * @details Once a socket is marked as listenable, it cannot be unmarked. It is important to
 *          consider the backlog paramter, as it will affect how much memory your application will
 *          use in the worst case.
 *
 * @param[in] sock      The socket descriptor on which to set listening options.
 * @param[in] backlog   The max length of the queue of pending connections. A value of 0 means
 *                      infinite.
 * @retval 0 on success, or -1 on error.
 */
int listen(int sock, int backlog);

/**
 * @brief Wait for next client to connect.
 *
 * @details This function will block if there are no clients attempting to connect.
 *
 * @param[in]  sock         The socket descriptor to use for waiting on client connections.
 * @param[out] p_cliaddr    Socket address that will be set to the clients address.
 * @param[out] p_addrlen    The size of the p_cliaddr passed. Might be modified by the function.
 *
 * @retval  A non-negative client desciptor on success, or -1 on error.
 */
int accept(int sock, void * p_cliaddr, socklen_t * p_addrlen);

/**
 * @brief Convert human readable ip address to a form usable by the socket api.
 *
 * @details This function will convert a string form of addresses and encode it into a byte array.
 *
 * @param[in]  af           Address family. Only AF_INET6 supported.
 * @param[in]  src          Null-terminated string containing the address to convert.
 * @param[out] dst          Pointer to a struct in6_addr where address will be stored.
 *
 * @retval 1 on success, 0 if src does not contain a valid address, -1 if af is not a valid address
 *         family.
 */
int inet_pton(socket_family_t af, const char * src, void * dst);

#endif //SOCKET_API_H__

/**@} */
