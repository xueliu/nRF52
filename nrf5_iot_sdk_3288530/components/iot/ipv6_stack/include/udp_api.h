/* Copyright (c)  2014 Nordic Semiconductor. All Rights Reserved.
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
 
/** @file udp_api.h
 *
 * @defgroup iot_udp UDP Application Interface for Nordic's IPv6 stack
 * @ingroup iot_sdk_stack
 * @{
 * @brief Nordic User Datagram Protocol Application Interface for Nordic's IPv6 stack.
 *
 * @details This module provides basic features related to User Datagram Protocol (UDP) support. 
 */

#ifndef UDP_API_H__
#define UDP_API_H__

#include "sdk_config.h"
#include "sdk_common.h"
#include "iot_defines.h"
#include "ipv6_api.h"

/**
 * @brief UDP socket reference.
 */
typedef struct
{
  uint32_t     socket_id;                                                                           /**< UDP socket identifier. */
  void       * p_app_data;                                                                          /**< Pointer to application data mapped by the application to the socket. If no mapping is provided by the application using the @ref udp6_socket_app_data_set API, this pointer is NULL. */
} udp6_socket_t;

/**
 * @brief   UDP data receive callback.
 *
 * @details API used to notify the application of UDP packets received. If the received data is
 *          malformed (for example, a checksum error), the packet is still notified to the application.
 *          The process_result parameter indicates whether the packet was successfully processed by UDP.
 *          The application should check process_result before
 *          consuming the packet.
 *
 * @param[in]  p_socket       Reference to the socket on which the data is received.
 * @param[in]  p_ip6_header   Pointer to the IP header of the received ICMP packet.
 * @param[in]  p_udp_header   Pointer to the UDP header of the received packet.
 * @param[in]  process_result Notifies the application if the UDP packet was processed successfully success or
 *                            if an error occurred, for example, the packet was malformed.
 * @param[in]  p_rx_packet    Packet buffer containing the received packed. p_rx_packet->p_payload
 *                            contains the UDP payload.
 *
 * @returns A provision for the application to notify the module of whether the received packet was
 *          processed successfully by application. The application may take ownership of the received
 *          packet by returning IOT_IPV6_ERR_PENDING, in which case the application must take care to
 *          free it using @ref iot_pbuffer_free.
 */
typedef uint32_t (* udp6_handler_t)(const udp6_socket_t * p_socket,
                                    const ipv6_header_t * p_ip_header,
                                    const udp6_header_t * p_udp_header,
                                    uint32_t              process_result,
                                    iot_pbuffer_t       * p_rx_packet);


/**
 * @brief   Allocates a UDP socket.
 *
 * @details This API should be called to be assigned a UDP socket. The maximum number of sockets that can
 *          be allocated using the API is determined by the define UDP6_MAX_SOCKET_COUNT.
 *
 * @param[out]  p_socket   Reference to the allocated socket is provided in the pointer if the procedure
 *                         was successful. Should not be NULL.
 *
 * @retval NRF_SUCCESS If the socket was allocated successfully. Otherwise, an 
 * error code that indicates the reason for the failure is returned.
 */
uint32_t udp6_socket_allocate(udp6_socket_t * p_socket);


/**
 * @brief   Frees an allocated UDP socket.
 *
 * @details API used to free a socket allocated using @ref udp6_socket_allocate.
 *
 * @param[in] p_socket Handle reference to the socket. Should not be NULL.
 *
 * @retval NRF_SUCCESS If the socket was freed successfully. Otherwise, an 
 * error code that indicates the reason for the failure is returned.
 *
 */
uint32_t udp6_socket_free(const udp6_socket_t * p_socket);


/**
 * @brief   Registers callback to be notified of data received on a socket.
 *
 * @details API to register a callback to be notified of data received on a socket.
 *
 * @param[in]  p_socket   Handle reference to the socket. Should not be NULL.
 * @param[in]  callback   Callback being registered to receive data. Should not be NULL.
 *
 * @retval NRF_SUCCESS If the procedure was executed successfully. Otherwise, an
 * error code that indicates the reason for the failure is returned.
 *
 */
uint32_t udp6_socket_recv(const udp6_socket_t  * p_socket,
                          const udp6_handler_t   callback);


/**
 * @brief   Binds a UDP socket to a specific port and address.
 *
 * @details API used to bind a UDP socket to a local port and an address.
 *
 * @param[in]  p_socket    Handle reference to the socket. Should not be NULL.
 * @param[in]  p_src_addr  Local IPv6 address to be bound on specific socket.
 * @param[in]  src_port    Local UDP port to be bound on specific socket.
 *
 * @retval NRF_SUCCESS If the procedure was executed successfully. Otherwise, an
 * error code that indicates the reason for the failure is returned.
 *
 */
uint32_t udp6_socket_bind(const udp6_socket_t * p_socket,
                          const ipv6_addr_t   * p_src_addr,
                          uint16_t              src_port);


/**
 * @brief   Connects a UDP socket to aspecific port and address.
 *
 * @details API used to connect a UDP socket to a remote port and remote address.
 *
 * @param[in]  p_socket    Handle reference to the socket. Should not be NULL.
 * @param[in]  p_dest_addr IPv6 address of the remote destination.
 * @param[in]  dest_port   Remote USP port to connect the socket to.
 *
 * @retval NRF_SUCCESS If the connection was established successfully.
 */
uint32_t udp6_socket_connect(const udp6_socket_t * p_socket,
                             const ipv6_addr_t   * p_dest_addr,
                             uint16_t              dest_port);


/**
 * @brief   Sends a UDP packet on a specific socket.
 *
 * @details API used to send UDP data over a UDP socket. Remote port and address must be set with
 *          \ref udp6_socket_connect() before using this API.
 *
 * Applications that call this function should allocate a packet with type 
 * UDP6_PACKET_TYPE (set in the allocation
 *       parameter) before calling the function.
 *
 * The application shall not free the allocated packet buffer if the procedure was
 *       successful, to ensure that no data copies are needed when transmitting a packet.
 *
 * @param[in]  p_socket   Handle reference to the socket. Should not be NULL.
 * @param[in]  p_packet   Data to be transmitted on the socket. The application should allocate a packet
 *                        buffer with type UDP6_PACKET_TYPE using \ref iot_pbuffer_allocate.
 *                        p_packet->p_payload and p_packet->length should be appropriately
 *                        populated by the application to contain the payload and length of the UDP
 *                        packet, respectively.
 *
  * @retval NRF_SUCCESS If the procedure was executed successfully. Otherwise, an
 * error code that indicates the reason for the failure is returned.
 *
 */
uint32_t udp6_socket_send(const udp6_socket_t * p_socket,
                          iot_pbuffer_t       * p_packet);


/**
 * @brief   Sends a UDP packet on a specific socket to a remote address and port.
 *
 * @details API used to send UDP data over a UDP socket.
 *
 * @param[in] p_socket     Handle reference to the socket. Should not be NULL.
 * @param[in] p_dest_addr  IPv6 address of the remote destination.
 * @param[in] dest_port    Remote UDP port to which data transmission is requested.
 * @param[in] p_packet     Data to be transmitted on the socket. Application should allocate a
 *                         packet buffer with type UDP6_PACKET_TYPE using \ref
 *                         iot_pbuffer_allocate. p_packet->p_payload and p_packet->length should
 *                         be appropriately populated by the application to contain the payload and
 *                         length of the UDP packet, respectively.
 *
 * @retval NRF_SUCCESS  If the procedure was executed successfully. Otherwise, an
 * error code that indicates the reason for the failure is returned.
 *
 */
uint32_t udp6_socket_sendto(const udp6_socket_t * p_socket,
                            const ipv6_addr_t   * p_dest_addr,
                            uint16_t              dest_port,
                            iot_pbuffer_t       * p_packet);


/**
 * @brief   Sets application data for a socket.
 *
 * @details A utility API that allows the application to set any application specific mapping with the
 *          UDP Socket. The UDP module remembers the pointer provided by the application as long as the
 *          socket is not freed and if receive data callback is called each time as part of
 *         udp_socket_t.
 *
 * @param[in]  p_socket  Pointer to the socket for which the application data mapping is being set.
 *                       A pointer to the application data should be provided by setting the 
 *                       p_socket->p_app_data field.
 *
 * @retval NRF_SUCCESS If the procedure was executed successfully. Otherwise, an
 * error code that indicates the reason for the failure is returned.
 *
 */
uint32_t udp6_socket_app_data_set(const udp6_socket_t * p_socket);

#endif //UDP_API_H__

/**@} */
