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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "lwm2m_api.h"
#include "lwm2m_bootstrap.h"
#include "lwm2m.h"
#include "coap_api.h"
#include "coap_message.h"
#include "coap_codes.h"
#include "sdk_config.h"
#include "app_util.h"

#define LWM2M_BOOTSTRAP_URI_PATH            "bs"

#define TOKEN_START                         0x012A

static uint16_t m_token = TOKEN_START;


static uint32_t internal_message_new(coap_message_t **        pp_msg, 
                                     coap_msg_code_t          code, 
                                     coap_response_callback_t callback,
                                     uint16_t                 local_port)
{
    uint32_t err_code;
    coap_message_conf_t conf;
    memset (&conf, 0, sizeof(coap_message_conf_t));
    
    conf.type              = COAP_TYPE_CON;
    conf.code              = code;
    conf.response_callback = callback;
    conf.port.port_number  = local_port;

    conf.token_len = uint16_encode(m_token, conf.token);
    
    m_token++;
    
    err_code = coap_message_new(pp_msg, &conf);

    return err_code;
}


/**@brief Function to be used as callback function upon a bootstrap request. */
static void lwm2m_bootstrap_cb(uint32_t status, void * p_arg, coap_message_t * p_message)
{
    LWM2M_TRC("[LWM2M][Bootstrap ]: lwm2m_bootstrap_cb, status: %ul, coap code: %u\r\n", 
              status, 
              p_message->header.code);
    
    (void)lwm2m_notification(LWM2M_NOTIFCATION_TYPE_BOOTSTRAP, 
                             &p_message->remote, 
                             p_message->header.code);
}


uint32_t internal_lwm2m_bootstrap_init(void)
{
    m_token = TOKEN_START;
    return NRF_SUCCESS;
}


uint32_t lwm2m_bootstrap(lwm2m_remote_t * p_remote, lwm2m_client_identity_t * p_id, uint16_t local_port)
{
    LWM2M_TRC("[LWM2M][Bootstrap ]: >> lwm2m_bootstrap\r\n");

    NULL_PARAM_CHECK(p_remote);
    NULL_PARAM_CHECK(p_id);

    LWM2M_MUTEX_LOCK();
    
    uint32_t         err_code;
    coap_message_t * p_msg;

    lwm2m_string_t endpoint;
    
    endpoint.p_val = LWM2M_BOOTSTRAP_URI_PATH;
    endpoint.len   = 2;

    err_code = internal_message_new(&p_msg, COAP_CODE_POST, lwm2m_bootstrap_cb, local_port);
    if(err_code != NRF_SUCCESS)
    {
        LWM2M_MUTEX_UNLOCK();
        return err_code;
    }
    
    if (err_code == NRF_SUCCESS)
    {
        err_code = coap_message_remote_addr_set(p_msg, p_remote);
    }
    
    if (err_code == NRF_SUCCESS)
    {
        err_code = coap_message_opt_str_add(p_msg, 
                                            COAP_OPT_URI_PATH, 
                                            (uint8_t *)endpoint.p_val, 
                                            endpoint.len); // end_point length is always 2
    }
    
    if (err_code == NRF_SUCCESS)
    {
        char buffer[40];
        buffer[0] = 'e';
        buffer[1] = 'p';
        buffer[2] = '=';
        memcpy(buffer + 3, &p_id->value, p_id->type);

        err_code = coap_message_opt_str_add(p_msg, 
                                            COAP_OPT_URI_QUERY, 
                                            (uint8_t *)buffer, 
                                            p_id->type + 3);
    }
    
    if (err_code == NRF_SUCCESS)
    {
        uint32_t msg_handle;
        err_code = coap_message_send(&msg_handle, p_msg);
    }
    
    if (err_code == NRF_SUCCESS)
    {
        err_code = coap_message_delete(p_msg);
    }
    else
    {
        // If we have hit an error try to clean up. 
        // Return the original error code.
        (void)coap_message_delete(p_msg);
    }
    
    LWM2M_TRC("[LWM2M][Bootstrap ]: << lwm2m_bootstrap\r\n");
    
    LWM2M_MUTEX_UNLOCK();
    
    return err_code;
}
