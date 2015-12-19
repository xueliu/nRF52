/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
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

/** @file coap_transport.h
 *
 * @defgroup iot_sdk_coap_transport CoAP transport abstraction
 * @ingroup iot_sdk_coap
 * @{
 * @brief The transport interface that the CoAP depends on for sending and receiving CoAP messages.
 *
 * @details  While the interface is well defined and should not be altered, the implementation of the
 *        interface depends on the choice of IP stack. The only exception to this is the
 *        \ref coap_transport_read API. This API is implemented in the CoAP, and the transport layer is
 *        expected to call this function when data is received on one of the CoAP ports.
 */

#ifndef COAP_TRANSPORT_H__
#define COAP_TRANSPORT_H__

#include <stdint.h>
#include <nrf_tls.h>


/**@brief Port identification information. */
typedef struct
{
    uint16_t      port_number;                       /**< Port number. */
} coap_port_t;


/**@brief Remote endpoint. */
typedef struct
{
    uint8_t        addr[16];                        /**< Address of the remote device. */
    uint16_t       port_number;                     /**< Remote port number. */ 
} coap_remote_t;


/**@brief Transport initialization information. */
typedef struct
{
    coap_port_t     * p_port_table;                 /**< Information about the ports being registered. Count is assumed to be COAP_PORT_COUNT. */
} coap_transport_init_t;



/**@brief Initializes the transport layer to have the data ports set up for CoAP.
 *
 * @param[in] p_param  Port count and port numbers.
 *
 * @retval NRF_SUCCESS If initialization was successful. Otherwise, an error code that indicates the reason for the failure is returned.
 */
uint32_t coap_transport_init (const coap_transport_init_t   * p_param);


/**@brief Sends data on a CoAP endpoint or port.
 *
 * @param[in] p_port    Port on which the data is to be sent.
 * @param[in] p_remote  Remote endpoint to which the data is targeted.
 * @param[in] p_data    Pointer to the data to be sent.
 * @param[in] datalen   Length of the data to be sent.
 *
 * @retval NRF_SUCCESS If the data was sent successfully. Otherwise, an error code that indicates the reason for the failure is returned.
 */
uint32_t coap_transport_write(const coap_port_t    * p_port,
                              const coap_remote_t  * p_remote,
                              const uint8_t        * p_data,
                              uint16_t               datalen);



/**@brief Handles data received on a CoAP endpoint or port.
 *
 * This API is not implemented by the transport layer, but assumed to exist. This approach
 * avoids unnecessary registering of callback and remembering it in the transport layer.
 *
 * @param[in] p_port    Port on which the data is to be sent.
 * @param[in] p_remote  Remote endpoint to which the data is targeted.
 * @param[in] result    Indicates if the data was processed successfully by lower layers.
 *                      Possible failures could be NRF_SUCCES, 
 *                                                 UDP_BAD_CHECKSUM,
 *                                                 UDP_TRUNCATED_PACKET, or
 *                                                 UDP_MALFORMED_PACKET.
 * @param[in] p_data    Pointer to the data to be sent.
 * @param[in] datalen   Length of the data to be sent.
 *
 * @retval NRF_SUCCESS If the data was handled successfully. Otherwise, an error code that indicates the reason for the failure is returned.
 *
 */
uint32_t coap_transport_read(const coap_port_t    * p_port,
                             const coap_remote_t  * p_remote,
                             uint32_t               result,
                             const uint8_t        * p_data,
                             uint16_t               datalen);
                             
void coap_transport_process(void);

#endif //COAP_TRANSPORT_H__

/** @} */
