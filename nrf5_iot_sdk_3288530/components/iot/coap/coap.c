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

#include <stdbool.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "coap_api.h"
#include "coap.h"
#include "coap_queue.h"
#include "coap_transport.h"
#include "app_trace.h"
#include "sdk_common.h"
#include "iot_common.h"
#include "ble_ipsp.h"
#include "mem_manager.h"
#include "coap_resource.h"
#include "coap_observe_api.h"
#include "coap_observe.h"

#define COAP_REQUEST_ENTITY_MAX_SIZE (BLE_IPSP_RX_BUFFER_SIZE - (IPV6_IP_HEADER_SIZE + \
                                                                 UDP_HEADER_SIZE))       /** Maximum request entity size. */

SDK_MUTEX_DEFINE(m_coap_mutex)                                                           /**< Mutex variable. Currently unused, this declaration does not occupy any space in RAM. */

static uint32_t              m_token_seed;                                               /**< Token seed provided by application to be used for generating token numbers. */
static uint32_t              m_message_id_counter;                                       /**< Message ID counter, used to generate unique message IDs. */
static coap_error_callback_t m_error_callback;                                           /**< Function pointer to an application CoAP error handler. */

static coap_request_handler_t m_request_handler = NULL;                                  /**< Request handler where to forward all incomming requests. */

#ifdef COAP_AUTOMODE

static uint32_t m_observe_sequence_number = 99;

#endif //#ifdef COAP_AUTOMODE

#define COAP_MESSAGE_ACK_SET(REMOTE, LOCAL_PORT, MID) {                    \
    memcpy(&m_coap_empty_message.remote, (REMOTE), sizeof(coap_remote_t)); \
    m_coap_empty_message.port.port_number = (LOCAL_PORT);                  \
    m_coap_empty_message.header.id        = (MID);                         \
    m_coap_empty_message.header.type      = COAP_TYPE_ACK;                 \
}

#define COAP_MESSAGE_RST_SET(REMOTE, LOCAL_PORT, MID) {                    \
    memcpy(&m_coap_empty_message.remote, (REMOTE), sizeof(coap_remote_t)); \
    m_coap_empty_message.port.port_number = (LOCAL_PORT);                  \
    m_coap_empty_message.header.id        = (MID);                         \
    m_coap_empty_message.header.type      = COAP_TYPE_RST;                 \
}

static coap_message_t m_coap_empty_message = {
    .header.version    = 1,
    .header.type       = COAP_TYPE_ACK,
    .header.token_len  = 0,
    .header.code       = COAP_CODE_EMPTY_MESSAGE,
    .header.id         = 0,
    .p_payload         = NULL,
    .payload_len       = 0,
    .options_count     = 0,
    .p_arg             = NULL,
    .response_callback = NULL,
    .port.port_number  = 0,
    .options_len       = 0,
    .options_offset    = 0,
    .p_data            = NULL,
    .data_len          = 0  
};

static inline bool is_ping(coap_message_t * p_message)
{
    return (p_message->header.code == COAP_CODE_EMPTY_MESSAGE) &&
           (p_message->header.type == COAP_TYPE_CON);
}

static inline bool is_ack(coap_message_t * p_message)
{
    return (p_message->header.code == COAP_CODE_EMPTY_MESSAGE) &&
           (p_message->header.type == COAP_TYPE_ACK);
}

static inline bool is_reset(coap_message_t * p_message)
{
    return (p_message->header.type == COAP_TYPE_RST);
}

static inline bool is_con_response(coap_message_t * p_message)
{
    return (p_message->header.code == COAP_CODE_205_CONTENT) &&
           (p_message->header.type == COAP_TYPE_CON);
}

static inline bool is_request(uint8_t message_code)
{
    return (message_code >= 1) && (message_code < 32);
}

static inline bool is_response(uint8_t message_code)
{
    return (message_code >= 64) && (message_code < 192);
}

static inline void app_error_notify(uint32_t err_code, coap_message_t * p_message)
{
    if (m_error_callback != NULL)
    {
        COAP_MUTEX_UNLOCK();    
        
        m_error_callback(err_code, p_message);

        COAP_MUTEX_LOCK();    
    }
}

uint32_t coap_init(uint32_t token_rand_seed, coap_transport_init_t * p_transport_param)
{
    COAP_TRC("[COAP]: >> coap_init %lu.\r\n", token_rand_seed);
        
    uint32_t err_code;
    
    SDK_MUTEX_INIT(m_coap_mutex);

    COAP_MUTEX_LOCK();
    
    internal_coap_observe_init();
    
    m_error_callback = NULL;
    
    m_token_seed = token_rand_seed;
    (void)m_token_seed;
    
    m_message_id_counter = 1;

    err_code = coap_transport_init(p_transport_param);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    err_code = coap_queue_init();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = coap_resource_init();

    COAP_MUTEX_UNLOCK();

    COAP_TRC("[COAP]: << coap_init\r\n");

    return err_code;

}

#ifndef COAP_AUTOMODE
uint32_t coap_error_handler_register(coap_error_callback_t error_callback)
{
    // TODO: error handling, null pointer, module initilized etc. 
    COAP_MUTEX_LOCK();

    m_error_callback = error_callback;
    
    COAP_MUTEX_UNLOCK();

    return NRF_SUCCESS;   
}
#endif //#ifndef COAP_AUTOMODE

uint32_t internal_coap_message_send(uint32_t * p_handle, coap_message_t * p_message)
{
    if (p_message == NULL)
    {
        return (NRF_ERROR_NULL | IOT_COAP_ERR_BASE);
    }

    // Compiled away if COAP_ENABLE_OBSERVE_CLIENT is not set to 1.
    coap_observe_client_send_handle(p_message);
        
    COAP_TRC("[COAP]: >> message_send\r\n");

    // Fetch the expected length of the packet serialized by passing length of 0.
    uint16_t expected_length = 0;
    uint32_t err_code = coap_message_encode(p_message, NULL, &expected_length);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Allocate a buffer to serialize the message into.
    uint8_t * p_buffer;
    uint32_t request_length = expected_length;
    err_code = nrf_mem_reserve(&p_buffer, &request_length);
    if (err_code != NRF_SUCCESS)
    {
        COAP_TRC("[COAP]: p_buffer alloc error = 0x%08lX!\r\n", err_code);
        return err_code;
    }
    memset(p_buffer, 0, request_length);
    COAP_TRC("[COAP]: Alloc mem, p_buffer = %p\r\n", (uint8_t *)p_buffer);

    // Serialize the message.
    uint16_t buffer_length = (uint16_t)request_length;
    err_code = coap_message_encode(p_message, p_buffer, &buffer_length);
    if (err_code != NRF_SUCCESS)
    {
        COAP_TRC("[COAP]: Encode error!\r\n");
        COAP_TRC("[COAP]: Free mem, p_buffer = %p\r\n", p_buffer);
        UNUSED_VARIABLE(nrf_free(p_buffer));

        return err_code;
    }

    err_code = coap_transport_write(&p_message->port, &p_message->remote, p_buffer, buffer_length);

    if (err_code == NRF_SUCCESS)
    {   
        if (is_request(p_message->header.code) ||
            is_con_response(p_message))
        {
            coap_queue_item_t item;
            item.p_arg         = p_message->p_arg;
            item.mid           = p_message->header.id;
            item.callback      = p_message->response_callback;
            item.p_buffer      = p_buffer;
            item.buffer_len    = buffer_length;
            item.timeout_val   = COAP_ACK_TIMEOUT * COAP_ACK_RANDOM_FACTOR;

            if (p_message->header.type == COAP_TYPE_CON)
            {
                item.timeout       = item.timeout_val;
                item.retrans_count = 0;
            }
            else
            {   
                item.timeout       = COAP_MAX_TRANSMISSION_SPAN;
                item.retrans_count = COAP_MAX_RETRANSMIT_COUNT;
            }
            
            item.port          = p_message->port;
            item.token_len     = p_message->header.token_len;
            memcpy(&item.remote, &p_message->remote, sizeof(coap_remote_t));
            memcpy(item.token, p_message->token, p_message->header.token_len);
            err_code = coap_queue_add(&item);
            if (err_code != NRF_SUCCESS)
            {
                COAP_TRC("[COAP]: Message queue error = 0x%08lX!\r\n", err_code);
                return err_code;
            }
            *p_handle = item.handle;
        }
        else
        {
            *p_handle = COAP_MESSAGE_QUEUE_SIZE;
            
            COAP_TRC("[COAP]: Free mem, p_buffer = %p\r\n", p_buffer);
            UNUSED_VARIABLE(nrf_free(p_buffer));
        }
    }
    else
    {
        COAP_TRC("[COAP]: Free mem, p_buffer = %p\r\n", p_buffer);
        UNUSED_VARIABLE(nrf_free(p_buffer));
    }
    
    COAP_TRC("[COAP]: << message_send\r\n");

    return err_code;
}


static uint32_t create_response(coap_message_t ** pp_response, coap_message_t * p_request, uint16_t data_size)
{
    uint32_t err_code;
    
    // Allocate space for a new message.
    uint32_t size = sizeof(coap_message_t);
    err_code = nrf_mem_reserve((uint8_t **)pp_response, &size);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    coap_message_t * p_response = (*pp_response);
    
    memset(p_response, 0, sizeof(coap_message_t));
    COAP_TRC("[COAP]: Alloc mem, p_response = %p\r\n", (uint8_t *)p_response);

    if (data_size > 0)
    {
        // Allocate a scratch buffer for payload and options.
        size = data_size;
        err_code = nrf_mem_reserve(&(p_response->p_data), &size);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
        memset(p_response->p_data, 0, size);
        p_response->data_len = size;
        COAP_TRC("[COAP]: Alloc mem, p_response->p_data = %p\r\n", p_response->p_data);
    }
    
    coap_message_conf_t config;
    memset (&config, 0, sizeof(coap_message_conf_t));

    config.token_len        = p_request->header.token_len;
    config.id               = p_request->header.id;
    config.code             = COAP_CODE_404_NOT_FOUND;
    config.port.port_number = p_request->port.port_number;

    memcpy(config.token, p_request->token, p_request->header.token_len);
    
    if ((coap_msg_type_t)p_request->header.type == COAP_TYPE_CON)
    {
        config.type = COAP_TYPE_ACK; 
    }
    else
    {
        config.type = (coap_msg_type_t)p_request->header.type; 
    }
    
    err_code = coap_message_create(p_response, &config);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    (void)coap_message_remote_addr_set(p_response, &p_request->remote);
    
    return NRF_SUCCESS;
}


/**@brief Common function for sending response error message 
 *
 * @param[in] p_message Pointer to the original request message.
 * @param[in] code      CoAP message code to send in the reponse. 
 * 
 * @retval NRF_SUCCESS  If the response was sent out successfully. 
 */
static uint32_t send_error_response(coap_message_t * p_message, uint8_t code)
{
    coap_message_t * p_error_response = NULL;        

    uint32_t err_code = create_response(&p_error_response, p_message, COAP_MESSAGE_DATA_MAX_SIZE);
    if (err_code != NRF_SUCCESS)
    {
        // If message could not be created, notify the application.
        app_error_notify(err_code, p_message);
        return err_code;
    }
    
    // Set the response code.
    p_error_response->header.code = code;

    if (p_error_response != NULL)
    {
        uint32_t handle;
        err_code = internal_coap_message_send(&handle, p_error_response);

        COAP_TRC("[COAP]: Free mem, p_response->p_data = %p\r\n", p_error_response->p_data);
        UNUSED_VARIABLE(nrf_free(p_error_response->p_data));

        COAP_TRC("[COAP]: Free mem, p_response = %p\r\n", (uint8_t *)p_error_response);
        UNUSED_VARIABLE(nrf_free((uint8_t *)p_error_response));
    }
    
    return err_code;
}

uint32_t coap_transport_read(const coap_port_t    * p_port,
                             const coap_remote_t  * p_remote,
                             uint32_t               result,
                             const uint8_t        * p_data,
                             uint16_t               datalen)
{
    COAP_TRC("[COAP]: >> coap_transport_read\r\n");
    
    // Discard all packets if not success or truncated. 
    if (!(result == NRF_SUCCESS || result == UDP_TRUNCATED_PACKET))  
    {
        return NRF_SUCCESS;
    }
    
    uint32_t err_code;
    coap_message_t * p_message;

    // Allocate space for a new message.
    uint32_t size = sizeof(coap_message_t);
    err_code = nrf_mem_reserve((uint8_t **)&p_message, &size);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    memset(p_message, 0, sizeof(coap_message_t));
    COAP_TRC("[COAP]: Alloc mem, p_message = %p\r\n", (uint8_t *)p_message);

    err_code = coap_message_decode(p_message, p_data, datalen);
    if (err_code != NRF_SUCCESS)
    {
        app_error_notify(err_code, p_message);
        
        UNUSED_VARIABLE(nrf_free((uint8_t *)p_message));
        return err_code;
    }
    
    // Copy the remote address information.
    memcpy(&p_message->remote, p_remote, sizeof(coap_remote_t));

    // Copy the local port information.
    memcpy(&p_message->port, p_port, sizeof(coap_port_t));

    if (result == UDP_TRUNCATED_PACKET)
    {
#ifdef COAP_AUTOMODE
        coap_message_t * p_response;
        
        // create a response message.
        err_code = create_response(&p_response, p_message, 4); // 1 option, 1 byte ext delta, potential 2 byte integer for size1.
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
        
        (void)coap_message_opt_uint_add(p_response, COAP_OPT_SIZE1, COAP_REQUEST_ENTITY_MAX_SIZE);
        
        p_response->header.token_len = 0;
        p_response->payload_len      = 0;
        
        // Override 404 code.
        p_response->header.code = COAP_CODE_413_REQUEST_ENTITY_TOO_LARGE;
        p_response->header.type = COAP_TYPE_RST;

        uint32_t msg_handle;
        err_code = internal_coap_message_send(&msg_handle, p_response);
        
        COAP_TRC("[COAP]: Free mem, p_response->p_data = %p\r\n", p_response->p_data);
        UNUSED_VARIABLE(nrf_free(p_response->p_data));
        
        COAP_TRC("[COAP]: Free mem, p_response = %p\r\n", (uint8_t *)p_response);
        UNUSED_VARIABLE(nrf_free((uint8_t *)p_response));
#else 
        app_error_notify(result, p_message);
#endif //#ifdef COAP_AUTOMODE
    }
    else if (is_ping(p_message))
    {
        COAP_MESSAGE_RST_SET(&p_message->remote, p_message->port.port_number, p_message->header.id);
            
        uint32_t handle;
        err_code = internal_coap_message_send(&handle, &m_coap_empty_message);
    }
    else if (is_ack(p_message) || 
             is_reset(p_message))
    {
        // Populate the token with the one used sending, before passing it to the application.
        coap_queue_item_t * p_item = NULL;
        err_code = coap_queue_item_by_mid_get(&p_item, p_message->header.id);
        
        if (err_code == NRF_SUCCESS)
        {
            if (p_item->callback != NULL)
            {
                // As the token is missing from peer, it will be added before giving it to the application.
                memcpy(p_message->token, p_item->token, p_item->token_len);
                p_message->header.token_len = p_item->token_len;

                // Compiled away if COAP_ENABLE_OBSERVE_CLIENT is not set to 1.
                coap_observe_client_response_handle(p_message, p_item);

                COAP_TRC("[COAP]: >> application callback\r\n");

                COAP_MUTEX_UNLOCK();

                if (is_ack(p_message))
                {
                    p_item->callback(NRF_SUCCESS, p_item->p_arg, p_message);
                }
                else
                {
                    p_item->callback(COAP_TRANSMISSION_RESET_BY_PEER, p_item->p_arg, p_message);
                }

                COAP_MUTEX_LOCK();

                COAP_TRC("[COAP]: << application callback\r\n");
            }
            
            COAP_TRC("[COAP]: Free mem, p_item->p_buffer = %p\r\n", p_item->p_buffer);
            UNUSED_VARIABLE(nrf_free(p_item->p_buffer));

            // Remove the queue element, as a match occured.
            err_code = coap_queue_remove(p_item);
        }
    }
    else if (is_response(p_message->header.code))
    {
        COAP_TRC("[COAP]: CoAP message type: RESPONSE\r\n");

        // If this is a CON response, we have to send an ACK on the message id.
        if (p_message->header.type == COAP_TYPE_CON)
        {
            // create an ACK message and add the current MID.
            COAP_MESSAGE_ACK_SET(&p_message->remote, p_message->port.port_number, p_message->header.id);
            
            uint32_t handle;
            coap_message_t coap_ack_message = m_coap_empty_message;
            coap_ack_message.port.port_number = p_port->port_number;

            err_code = internal_coap_message_send(&handle, &coap_ack_message);
        }
        
        coap_queue_item_t * p_item;
        err_code = coap_queue_item_by_token_get(&p_item, p_message->token, p_message->header.token_len);
        if (err_code != NRF_SUCCESS)
        {
            // Compiled away if COAP_ENABLE_OBSERVE_CLIENT is not set to 1.
            coap_observe_client_response_handle(p_message, NULL);
            
            UNUSED_VARIABLE(nrf_free((uint8_t *)p_message));
            return err_code; 
        }

        if (p_item->callback != NULL)
        {
            // Compiled away if COAP_ENABLE_OBSERVE_CLIENT is not set to 1.
            coap_observe_client_response_handle(p_message, p_item);
            
            COAP_TRC("[COAP]: >> application callback\r\n");

            COAP_MUTEX_UNLOCK();

            p_item->callback(NRF_SUCCESS, p_item->p_arg, p_message);
            
            COAP_MUTEX_LOCK();

            COAP_TRC("[COAP]: << application callback\r\n");
        }
      
        COAP_TRC("[COAP]: Free mem, p_item->p_buffer = %p\r\n", p_item->p_buffer);
        UNUSED_VARIABLE(nrf_free(p_item->p_buffer));
        
        err_code = coap_queue_remove(p_item);
        
    }
    else if (is_request(p_message->header.code))
    {
        COAP_TRC("[COAP]: CoAP message type: REQUEST\r\n");

        if (m_request_handler != NULL)
        {
            uint32_t return_code = m_request_handler(p_message);
            
            // If success, then all processing and any responses has been sent 
            // by the application callback.
            
            // If not success, then send an appropriate error message back to the 
            // origin with the return_code from the callback.
            if (return_code != NRF_SUCCESS)
            {
                if (return_code == NRF_ERROR_NOT_FOUND)
                {
                    // Send response with provided CoAP code.
                    (void)send_error_response(p_message, COAP_CODE_404_NOT_FOUND);
                }
                else if (return_code == NRF_ERROR_NULL)
                {
                    (void)send_error_response(p_message, COAP_CODE_405_METHOD_NOT_ALLOWED);
                }
                else
                {
                    (void)send_error_response(p_message, COAP_CODE_400_BAD_REQUEST);
                }
            }
        }
        else
        {
            uint8_t * uri_pointers[COAP_RESOURCE_MAX_DEPTH] = {0, };

            uint8_t  uri_path_count = 0;
            uint16_t index;

            for (index = 0; index < p_message->options_count; index++)
            {
                if (p_message->options[index].number == COAP_OPT_URI_PATH)
                {
                    uri_pointers[uri_path_count++] = p_message->options[index].p_data;
                }
            }
            
            coap_resource_t * found_resource;
            err_code = coap_resource_get(&found_resource, uri_pointers, uri_path_count);
            
            if (found_resource != NULL)
            {
                if (found_resource->callback != NULL)
                {
                    if (((found_resource->permission) & (1 << ((p_message->header.code) - 1))) > 0) // Has permission for the requested CoAP method.
                    {
                        COAP_MUTEX_UNLOCK();
                        
                        found_resource->callback(found_resource, p_message);
                        
                        COAP_MUTEX_LOCK();
                    }
                    else
                    {
                        // Reply with Method Not Allowed.
                        err_code = send_error_response(p_message, COAP_CODE_405_METHOD_NOT_ALLOWED);
                    }
                }
                else
                {
                    // Reply with Method Not Allowed.
                    err_code = send_error_response(p_message, COAP_CODE_405_METHOD_NOT_ALLOWED);
                }
            }
            else
            {
                // Reply with NOT FOUND.
                err_code = send_error_response(p_message, COAP_CODE_404_NOT_FOUND);
            }
        }
    }

    COAP_TRC("[COAP]: Free mem, p_message = %p\r\n", (uint8_t *)p_message);
    UNUSED_VARIABLE(nrf_free((uint8_t *)p_message));

    COAP_TRC("[COAP]: << coap_transport_read\r\n");
    return err_code;
}

uint32_t coap_message_send(uint32_t * p_handle, coap_message_t * p_message)
{
    COAP_MUTEX_LOCK();

    uint32_t err_code = internal_coap_message_send(p_handle, p_message);
    
    COAP_MUTEX_UNLOCK();
    
    return err_code;
}

uint32_t coap_message_abort(uint32_t handle)
{
    
    return NRF_ERROR_NOT_SUPPORTED;
}

uint32_t coap_message_new(coap_message_t ** p_request, coap_message_conf_t * p_config)
{
    COAP_TRC("[COAP]: >> coap_request_create\r\n");

    COAP_MUTEX_LOCK();

    uint32_t err_code;

    // If port is not configured, return error and skip initialization of the message.
    if (p_config->port.port_number == 0)
    {
        return (NRF_ERROR_INVALID_PARAM | IOT_COAP_ERR_BASE);
    }

    // Allocate space for a new message.
    uint32_t size = sizeof(coap_message_t);
    err_code = nrf_mem_reserve((uint8_t **)p_request, &size);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    memset(*p_request, 0, sizeof(coap_message_t));
    COAP_TRC("[COAP]: Alloc mem, *p_request = %p\r\n", (uint8_t *)(*p_request));

    // Allocate a scratch buffer for payload and options.
    size = COAP_MESSAGE_DATA_MAX_SIZE;
    err_code = nrf_mem_reserve(&((*p_request)->p_data), &size);
    if (err_code != NRF_SUCCESS)
    {
        COAP_TRC("[COAP]: Allocation of message data buffer failed!\r\n");

        COAP_TRC("[COAP]: Free mem, *p_request = %p\r\n", (uint8_t *)(*p_request));
        UNUSED_VARIABLE(nrf_free((uint8_t *)(*p_request)));
        return err_code;
    }

    memset((*p_request)->p_data, 0, size);
    (*p_request)->data_len = size;
    
    COAP_TRC("[COAP]: Alloc mem, (*p_request)->p_data = %p\r\n", (uint8_t *)((*p_request)->p_data));
    
    if (p_config->id == 0) // Message id is not set, generate one.
    {
        p_config->id = m_message_id_counter++;
    }

    err_code = coap_message_create(*p_request, p_config);

    COAP_MUTEX_UNLOCK();

    COAP_TRC("[COAP]: << coap_request_create, result = 0x%08lX\r\n", err_code);

    return err_code;
}

uint32_t coap_message_delete(coap_message_t * p_message)
{
    COAP_TRC("[COAP]: >> coap_request_delete\r\n");

    COAP_MUTEX_LOCK();

    //If this is a request free the coap_message_t and the data buffer.

    COAP_TRC("[COAP]: Free mem, p_message->p_data = %p\r\n", p_message->p_data);
    UNUSED_VARIABLE(nrf_free(p_message->p_data));

    COAP_TRC("[COAP]: Free mem, p_message = %p\r\n", (uint8_t *)p_message);
    UNUSED_VARIABLE(nrf_free((uint8_t *)p_message));


    COAP_MUTEX_UNLOCK();

    COAP_TRC("[COAP]: << coap_request_delete\r\n");

    return NRF_SUCCESS;
}

#ifdef COAP_AUTOMODE

static uint32_t internal_observer_notify(coap_resource_t * p_resource)
{
    // Fetch all observers which are subscribed to this resource.
    // Then send an update value too each observer.
    coap_observer_t * p_observer = NULL;
    while (coap_observe_server_next_get(&p_observer, p_observer, p_resource) != (NRF_ERROR_NOT_FOUND | IOT_COAP_ERR_BASE))
    {
        // Generate a message.
        coap_message_conf_t response_config;
        memset(&response_config, 0, sizeof(coap_message_conf_t));
        
        response_config.type = COAP_TYPE_NON;
        response_config.code = COAP_CODE_205_CONTENT;

        // Copy token.
        memcpy(&response_config.token[0], &p_observer->token[0], p_observer->token_len);
        // Copy token length.
        response_config.token_len = p_observer->token_len;
        response_config.port.port_number = COAP_SERVER_PORT;
        
        coap_message_t * p_response;
        uint32_t err_code = coap_message_new(&p_response, &response_config);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
        
        err_code = coap_message_remote_addr_set(p_response, &p_observer->remote);
        if (err_code != NRF_SUCCESS)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }
        
        err_code = coap_message_opt_uint_add(p_response, COAP_OPT_OBSERVE, m_observe_sequence_number++);
        if (err_code != NRF_SUCCESS)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }
            
        err_code = coap_message_opt_uint_add(p_response, COAP_OPT_MAX_AGE, p_resource->expire_time);
        if (err_code != NRF_SUCCESS)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }
        
        COAP_MUTEX_UNLOCK();
        
        err_code = coap_resource_observe_payload_set(p_resource, p_observer->ct, p_response);
        
        COAP_MUTEX_LOCK();
        
        if (err_code != NRF_SUCCESS)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }
        
        uint32_t msg_handle;
        err_code = internal_coap_message_send(&msg_handle, p_response);
        if (err_code != NRF_SUCCESS)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }
        
        err_code = coap_message_delete(p_response);
        if (err_code != NRF_SUCCESS)
        {
            (void)coap_message_delete(p_response);
            return err_code;
        }
    }
    
    return NRF_SUCCESS;
}

uint32_t coap_observer_notify(coap_resource_t * p_resource)
{
    COAP_MUTEX_UNLOCK();

    uint32_t err_code = internal_observer_notify(p_resource);
    
    COAP_MUTEX_UNLOCK();
    
    return err_code;
}

static uint32_t resource_autonotify(coap_resource_t * p_current_resource)
{
    uint32_t err_code = NRF_SUCCESS;
    
    if (p_current_resource->p_front != NULL)
    {
        coap_resource_t * next_child = p_current_resource->p_front;
        do
        {
            err_code = resource_autonotify(next_child);
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }
            next_child = next_child->p_sibling;
        } while (next_child != NULL);
    }
    
    // If the resource is observable, append 'obs;' token.
    if ((p_current_resource->permission & COAP_PERM_OBSERVE) > 0)
    {
        if (p_current_resource->expire_time <= (0 + COAP_AUTOMODE_OBSERVE_NOTIFY_DELTA_MAX_AGE))
        {
            p_current_resource->expire_time = p_current_resource->max_age;
            
            (void)internal_observer_notify(p_current_resource);
        }
        else
        {
            p_current_resource->expire_time--;
        }
    }
    
    return err_code;
}

static uint32_t coap_observer_process_tick(void)
{
    coap_resource_t * root;
    
    COAP_MUTEX_LOCK();    
    
    uint32_t err_code = coap_resource_root_get(&root);
    
    if (err_code == NRF_SUCCESS)
    {
        resource_autonotify(root);
    }
    // If root was not found, we can safely return as there is 
    // nothing to process.
    
    COAP_MUTEX_UNLOCK();    

    return NRF_SUCCESS;
}
#endif

uint32_t coap_time_tick(void)
{
    COAP_MUTEX_LOCK();
    
    coap_transport_process();
    
#ifdef COAP_AUTOMODE
    uint32_t err_code = coap_observer_process_tick();
#endif
    
    // Loop through the message queue to see if any packets needs retransmission, or has timed out.
    coap_queue_item_t * p_item = NULL;
    while (coap_queue_item_next_get(&p_item, p_item) == NRF_SUCCESS)
    {
        if (p_item->timeout == 0)
        {                        
            // If there is still retransmission attempts left.
            if (p_item->retrans_count < COAP_MAX_RETRANSMIT_COUNT)
            {
                p_item->timeout     = p_item->timeout_val * 2;
                p_item->timeout_val = p_item->timeout;
                p_item->retrans_count++;
                
                // Retransmit the message.
                uint32_t err_code = coap_transport_write(&p_item->port, &p_item->remote, p_item->p_buffer, p_item->buffer_len);
                if (err_code != NRF_SUCCESS)
                {
                    app_error_notify(err_code, NULL);
                }
            }
            
            // No more retransmission attempts left, or max transmit span reached.
            if ((p_item->timeout > COAP_MAX_TRANSMISSION_SPAN) ||
                (p_item->retrans_count >= COAP_MAX_RETRANSMIT_COUNT))
            {
                
                COAP_MUTEX_UNLOCK();    
                
                p_item->callback(COAP_TRANSMISSION_TIMEOUT, p_item->p_arg, NULL);
                
                COAP_MUTEX_LOCK();
                
                COAP_TRC("[COAP]: Free mem, p_item->p_buffer = %p\r\n", p_item->p_buffer);
                UNUSED_VARIABLE(nrf_free(p_item->p_buffer));
                
                (void)coap_queue_remove(p_item);
            }
        }
        else
        {
            p_item->timeout--;
        }
    }
    
    COAP_MUTEX_UNLOCK();
    
    return NRF_SUCCESS;
}

uint32_t coap_request_handler_register(coap_request_handler_t p_request_handler)
{
    COAP_MUTEX_LOCK();
    
    m_request_handler = p_request_handler;
    
    COAP_MUTEX_UNLOCK();
    
    return NRF_SUCCESS;
}
