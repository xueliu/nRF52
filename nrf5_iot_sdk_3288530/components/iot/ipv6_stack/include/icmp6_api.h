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
 
/** @file icmp6_api.h
 *
 * @defgroup iot_icmp6 ICMP6 Application Interface for Nordic's IPv6 stack
 * @ingroup iot_sdk_stack
 * @{
 * @brief Nordic Internet Control Message Protocol Application Interface for Nordic's IPv6 stack.
 *
 * @details This module provides basic features related to ICMPv6 support.
 */

#ifndef ICMP6_API_H__
#define ICMP6_API_H__

#include "sdk_config.h"
#include "sdk_common.h"
#include "iot_defines.h"
#include "ipv6_api.h"

#define  ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET    4                                                     /**< Offset of echo request payload from ICMPv6 header. */

/** Neighbor solicitation parameters. */
typedef struct
{
    ipv6_addr_t    target_addr;          /**< The IPv6 address of the target of the solicitation. MUST NOT be a multicast address. */
    bool           add_aro;              /**< Indicates if ARO option should be added. */
    uint16_t       aro_lifetime;         /**< The amount of time in units of 60 seconds that the router should retain the NCE for the sender of the NS. */
}icmp6_ns_param_t;


/**@brief   ICMPv6 data RX callback.
 *
 * @details Asynchronous callback used to notify the application of ICMP packets received. By default,
 *          the application is not notified through ICMP of messages related to 
 *          ECHO requests or any errors. However, these notifications can easily
 *          be enabled using ICMP6_ENABLE_ND6_MESSAGES_TO_APPLICATION or 
 *          ICMP6_ENABLE_ALL_MESSAGES_TO_APPLICATION if the application should
 *          handle them.
 *
 * @param[in]  p_interface    Pointer to the IPv6 interface from where the ICMP packet was received.
 * @param[in]  p_ip_header    Pointer to the IP header of the ICMP packet received.
 * @param[in]  p_icmp_header  Pointer to the ICMP header of the received packet.
 * @param[in]  process_result Notifies the application if the ICMP packet was processed successfully or if
 *                            an error occurred, for example, if the packet was malformed.
 * @param[in]  p_rx_packet    Packet buffer containing the packet received. p_rx_packet->p_payload
 *                            contains the ICMP payload.
 *       
 * @returns A provision for the application to notify the module of whether the received packet was
 *          processed successfully by application. The application may take ownership of the received
 *          packet by returning IOT_IPV6_ERR_PENDING, in which case the application must take care to
 *          free it using @ref iot_pbuffer_free.
 */
typedef uint32_t (*icmp6_receive_callback_t)(iot_interface_t  * p_interface,
                                             ipv6_header_t    * p_ip_header,
                                             icmp6_header_t   * p_icmp_header,
                                             uint32_t           process_result,
                                             iot_pbuffer_t    * p_rx_packet);
				   
           
/**@brief   Sends ICMPv6 echo request as defined in RFC4443.
 *
 * @details API used to send an ICMPv6 echo request packet to a specific destination address. The user
            can decide how much additional data must be sent.
 *
 *  The application calling the function should allocate a packet before, 
 *  with the type set to ICMP6_PACKET_TYPE in the allocation parameter.
 *
 * The application should pack the payload at ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET. Sequence number,
 *       ICMP Code, type checksum, etc. are filled in by the module. 
 *       For example, if the application wants to send an echo request containing 10 'A's, 
 *       it should first allocate a buffer and then set the payload and payload length as follows:
 *
 *       @code
 *           memset(p_request->p_payload+ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET, 'A', 10);
 *           p_request->length = ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET+10;
 *       @endcode
 * 
 * The application shall not free the allocated packet buffer if the procedure was successful,
 *       to ensure that no data copies are needed when transmitting a packet.
 * 
 * @param[in]  p_interface      Pointer to the IPv6 interface to send the ICMP packet.
 * @param[in]  p_src_addr       IPv6 source address from where the echo request is sent.
 * @param[in]  p_dest_addr      IPv6 destination address to where the echo request is sent.
 * @param[in]  p_request        Packet buffer containing the echo request.
 *      
 * @retval NRF_SUCCESS If the send request was successful.
 */
uint32_t icmp6_echo_request(const iot_interface_t  * p_interface,
                            const ipv6_addr_t      * p_src_addr,
                            const ipv6_addr_t      * p_dest_addr,
                            iot_pbuffer_t          * p_request);
                               
                               
/**@brief   Sends router solicitation message defined in RFC6775.
 *
 * @details API used to send a neighbor discovery message of type Router Solicitation to a specific 
 *          destination address. If no address is known, the user should send the message to all routers'
 *          address (FF02::1).
 *
 * The function internally tries to allocate a packet buffer. EUI-64 used in the SLLAO option is taken from
 *       the interface parameter defined in the @ref ipv6_init() function.
 *
 * @param[in]  p_interface      Pointer to the IPv6 interface to send the ICMP packet.
 * @param[in]  p_src_addr       IPv6 source address from where the router solicitation message is sent.
 * @param[in]  p_dest_addr      IPv6 destination address to where the router solicitation message is sent.
 *     
 * @retval NRF_SUCCESS If the send request was successful.
 */                                                            
uint32_t icmp6_rs_send(const iot_interface_t  * p_interface,
                       const ipv6_addr_t      * p_src_addr,
                       const ipv6_addr_t      * p_dest_addr);

                       
/**@brief   Sends neighbour solicitation message defined in RFC6775.
 *
 * @details API used to send a neighbor discovery message of type Neighbor Solicitation to a specific
 *          destination address.
 *
 * The function internally tries to allocate a packet buffer. EUI-64 used in the SLLAO and ARO options is taken from
 *       the interface parameter defined in the @ref ipv6_init() function.
 *
 * @param[in]  p_interface      Pointer to the IPv6 interface to send the ICMP packet.
 * @param[in]  p_src_addr       IPv6 source address from where the neighbor solicitation message is sent.
 * @param[in]  p_dest_addr      IPv6 destination address to where the neighbor solicitation message is sent.
 * @param[in]  p_ns_param       Neighbor discovery parameters.
 *

 * @retval NRF_SUCCESS  If the send request was successful.
 */                                                            
uint32_t icmp6_ns_send(const iot_interface_t  * p_interface,
                       const ipv6_addr_t      * p_src_addr,
                       const ipv6_addr_t      * p_dest_addr,
                       const icmp6_ns_param_t * p_ns_param);                 
                             
                             
/**@brief   Registers the callback function for echo reply. 
 *
 * @details API used to register callback to indicate the ICMP echo reply packet. Could be not used.
 *
 * Neighbor discovery related messages are not relayed to the application by default.
 *       However, this can be enabled by using the ICMP6_ENABLE_ND6_MESSAGES_TO_APPLICATION
 *       configuration parameter.
 *
 * @param[in]  cb  Handler called when an ICMP packet is received.
 *
 * @retval NRF_SUCCESS If the registration was successful.
 */
uint32_t icmp6_receive_register(icmp6_receive_callback_t cb);

#endif //ICMP6_API_H__

/**@} */
