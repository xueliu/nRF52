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

#include <stdint.h>
#include "sdk_errors.h"
#include "sdk_config.h"
#include "iot_common.h"
#include "coap_transport.h"
#include "coap.h"
#include "lwip/ip6_addr.h"
/*lint -save -e607 Suppress warning 607 "Parameter p of macro found within string" */
#include "lwip/udp.h"
/*lint -restore */


/**@brief UDP port information. */
typedef struct
{
    struct udp_pcb  * p_socket;                            /**< Socket information provided by UDP. */
    uint16_t          port_number;                         /**< Associated port number. */
}udp_port_t;

static udp_port_t m_port_table[COAP_PORT_COUNT];           /**< Table maintaining association between CoAP ports and corresponding UDP socket identifiers. */


/**@brief Callback handler to receive data on the UDP port.
 *
 * @details Callback handler to receive data on the UDP port.
 *
 * @param[in]   p_arg            Receive argument associated with the UDP socket.
 * @param[in]   p_socket         Socket identifier.
 * @param[in]   p_ip_header      IPv6 header containing source and destination addresses.
 * @param[in]   p_remote_addr    IPv6 address of the remote device.
 * @param[in]   port             Port number identifying the remote endpoint.
 *
 * @retval NRF_SUCCESS          Indicates received data was handled successfully, else an an
 *                              error code indicating reason for failure.
 */
static void udp_recv_data_handler(void           * p_arg,
                                  struct udp_pcb * p_socket,
                                  struct pbuf    * p_buffer,
                                  ip6_addr_t     * p_remote_addr,
                                  u16_t            port)
{
    uint32_t                index;
    coap_remote_t           remote_endpoint;
    coap_port_t             local_port = {p_socket->local_port};

    for(index = 0; index < COAP_PORT_COUNT; index++)
    {
        if(m_port_table[index].p_socket == p_socket)
        {
            memcpy (remote_endpoint.addr, p_remote_addr, 16);
            remote_endpoint.port_number = port;

            COAP_MUTEX_LOCK();

            UNUSED_VARIABLE(coap_transport_read(&local_port,
                                         &remote_endpoint,
                                         NRF_SUCCESS,
                                         (uint8_t *)p_buffer->payload,
                                         (uint32_t)p_buffer->len));
 
            COAP_MUTEX_UNLOCK();

            break;
        }
    }

    //Freeing packet (irrespective of matching p_socket is found or not
    //to avoid memory leaks in the system.
    UNUSED_VARIABLE(pbuf_free(p_buffer));
}


/**@brief Creates port as requested in p_port.
 *
 * @details Creates port as requested in p_port.
 *
 * @param[in]   index    Index to the m_port_table where entry of the port created is to be made.
 * @param[in]   p_port   Port information to be created.
 *
 * @retval NRF_SUCCESS   Indicates if port was created successfully, else an an  error code
 *                       indicating reason for failure.
 */
static uint32_t port_create(uint32_t index, coap_port_t  * p_port)
{
    err_t           err = NRF_ERROR_NO_MEM;
    ip6_addr_t      any_addr;
    struct udp_pcb * p_socket = m_port_table[index].p_socket;

    ip6_addr_set_any(&any_addr);

    //Request new socket creation.
    p_socket = udp_new_ip6();

    if (NULL != p_socket)
    {
        // Bind the socket to the local port.
        err = udp_bind_ip6(p_socket, &any_addr, p_port->port_number);
        if (err == ERR_OK)
        {
            //Register data receive callback.
            udp_recv_ip6(p_socket, udp_recv_data_handler, &m_port_table[index]);
            //All procedure with respect to port creation succeeded, make entry in the table.
            m_port_table[index].port_number = p_port->port_number;
            m_port_table[index].p_socket    = p_socket;
        }
        else
        {
            //Not all procedures succeeded with allocated socket, hence free it.
            err = NRF_ERROR_INVALID_PARAM;
            udp_remove(p_socket);
        }
    }

    return err;
}


uint32_t coap_transport_init(const coap_transport_init_t   * p_param)
{
    uint32_t    err_code = NRF_SUCCESS;
    uint32_t    index;

    NULL_PARAM_CHECK(p_param);
    NULL_PARAM_CHECK(p_param->p_port_table);

    for(index = 0; index < COAP_PORT_COUNT; index++)
    {
        // Create end point for each of the COAP ports.
        err_code = port_create(index, &p_param->p_port_table[index]);
        if (err_code != NRF_SUCCESS)
        {
            break;
        }
    }

    return err_code;
}


uint32_t coap_transport_write(const coap_port_t    * p_port,
                              const coap_remote_t  * p_remote,
                              const uint8_t        * p_data,
                              uint16_t               datalen)
{

    err_t err = NRF_ERROR_NOT_FOUND;
    uint32_t index;

    NULL_PARAM_CHECK(p_port);
    NULL_PARAM_CHECK(p_remote);
    NULL_PARAM_CHECK(p_data);

	//Search for the corresponding port.
    for (index = 0; index < COAP_PORT_COUNT; index++)
    {
        if (m_port_table[index].port_number == p_port->port_number)
        {
		    //Allocate Buffer to send the data on port.
            struct pbuf * lwip_buffer = pbuf_alloc(PBUF_TRANSPORT, datalen, PBUF_RAM);

            if (NULL != lwip_buffer)
            {
			    //Make a copy of the data onto the buffer.
                memcpy(lwip_buffer->payload, p_data, datalen);

                COAP_MUTEX_UNLOCK();

				//Send on UDP port.
                err = udp_sendto_ip6(m_port_table[index].p_socket,
                                    lwip_buffer,
                                    p_remote->addr,
                                    p_remote->port_number);

                COAP_MUTEX_LOCK();


                if (err != ERR_OK)
                {
				    //Free the allocated buffer as send procedure has failed.                   
                    err = NRF_ERROR_INTERNAL;
                }
                UNUSED_VARIABLE(pbuf_free(lwip_buffer));
            }
            else
            {
			    //Buffer allocation failed, cannot send data.
                err = NRF_ERROR_NO_MEM;
            }
            break;
        }
    }
    return err;
}


void coap_transport_process(void)
{
    return;
}


uint32_t coap_security_setup(uint16_t                       local_port,
                             nrf_tls_role_t                 role,
                             coap_remote_t          * const p_remote,
                             nrf_tls_key_settings_t * const p_settings)
{
    return API_NOT_IMPLEMENTED;
}


uint32_t coap_security_destroy(uint16_t              local_port,
                               coap_remote_t * const p_remote)
{
    return API_NOT_IMPLEMENTED;
}
