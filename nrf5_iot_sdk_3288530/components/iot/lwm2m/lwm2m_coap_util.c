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
#include "coap_api.h"
#include "coap_message.h"
#include "coap_codes.h"
#include "lwm2m.h"


uint32_t lwm2m_respond_with_code(coap_msg_code_t code, coap_message_t * p_request)
{
    NULL_PARAM_CHECK(p_request);
    
    // Application helper function, no need for mutex.
    coap_message_conf_t response_config;
    memset (&response_config, 0, sizeof(coap_message_conf_t));

    if (p_request->header.type == COAP_TYPE_NON)
    {
        response_config.type = COAP_TYPE_NON;
    }
    else if (p_request->header.type == COAP_TYPE_CON)
    {
        response_config.type = COAP_TYPE_ACK;
    }

    // PIGGY BACKED RESPONSE
    response_config.code              = code;
    response_config.id                = p_request->header.id;
    response_config.port.port_number  = p_request->port.port_number;

    // Copy token.
    memcpy(&response_config.token[0], &p_request->token[0], p_request->header.token_len);
    // Copy token length.
    response_config.token_len = p_request->header.token_len;

    coap_message_t * p_response;
    uint32_t err_code = coap_message_new(&p_response, &response_config);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = coap_message_remote_addr_set(p_response, &p_request->remote);
    if (err_code != NRF_SUCCESS)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    memcpy(&p_response->remote, &p_request->remote, sizeof(coap_remote_t));

    uint32_t msg_handle;
    err_code = coap_message_send(&msg_handle, p_response);
    if (err_code != NRF_SUCCESS)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_delete(p_response);

    return err_code;
}


uint32_t lwm2m_respond_with_payload(uint8_t * p_payload, uint16_t payload_len, coap_message_t * p_request)
{
    NULL_PARAM_CHECK(p_request);
    NULL_PARAM_CHECK(p_payload);
    
    // Application helper function, no need for mutex.
    coap_message_conf_t response_config;
    memset (&response_config, 0, sizeof(coap_message_conf_t));

    if (p_request->header.type == COAP_TYPE_NON)
    {
        response_config.type = COAP_TYPE_NON;
    }
    else if (p_request->header.type == COAP_TYPE_CON)
    {
        response_config.type = COAP_TYPE_ACK;
    }

    // PIGGY BACKED RESPONSE
    response_config.code              = COAP_CODE_205_CONTENT;
    response_config.id                = p_request->header.id;
    response_config.port.port_number  = p_request->port.port_number;
    
    // Copy token.
    memcpy(&response_config.token[0], &p_request->token[0], p_request->header.token_len);
    // Copy token length.
    response_config.token_len = p_request->header.token_len;

    coap_message_t * p_response;
    uint32_t err_code = coap_message_new(&p_response, &response_config);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = coap_message_payload_set(p_response, p_payload, payload_len);
    if (err_code != NRF_SUCCESS)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_remote_addr_set(p_response, &p_request->remote);
    if (err_code != NRF_SUCCESS)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    memcpy(&p_response->remote, &p_request->remote, sizeof(coap_remote_t));

    uint32_t msg_handle;
    err_code = coap_message_send(&msg_handle, p_response);
    if (err_code != NRF_SUCCESS)
    {
        (void)coap_message_delete(p_response);
        return err_code;
    }

    err_code = coap_message_delete(p_response);

    return err_code;
}
