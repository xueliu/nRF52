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

#include "string.h"
#include "coap_queue.h"
#include "iot_common.h"
#include "sdk_config.h"

#if (COAP_DISABLE_API_PARAM_CHECK == 0)

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL | IOT_COAP_ERR_BASE);                                               \
    }
#else 
    
#define NULL_PARAM_CHECK(PARAM)

#endif // COAP_DISABLE_API_PARAM_CHECK 

static coap_queue_item_t m_queue[COAP_MESSAGE_QUEUE_SIZE];
static uint8_t           m_message_queue_count = 0;

uint32_t coap_queue_init(void)
{
    for (uint8_t i = 0; i < COAP_MESSAGE_QUEUE_SIZE; i++)
    {
        memset(&m_queue[i], 0, sizeof(coap_queue_item_t));
        m_queue[i].handle = i;
    }
    m_message_queue_count = 0;
    
    return NRF_SUCCESS;
}

uint32_t coap_queue_add(coap_queue_item_t * item)
{
    NULL_PARAM_CHECK(item);
    
    if (m_message_queue_count >= COAP_MESSAGE_QUEUE_SIZE)
    {
       return (NRF_ERROR_NO_MEM | IOT_COAP_ERR_BASE);
    }
    else 
    {
        for (uint8_t i = 0; i < COAP_MESSAGE_QUEUE_SIZE; i++)
        {
            if (m_queue[i].p_buffer == NULL)
            {
                // Free spot in message queue. Add message here...                
                memcpy(&m_queue[i], item, sizeof(coap_queue_item_t));
                
                m_message_queue_count++;

                return NRF_SUCCESS;
            }
        }
        
    }
    return (NRF_ERROR_DATA_SIZE | IOT_COAP_ERR_BASE);
}

uint32_t coap_queue_remove(coap_queue_item_t * p_item)
{
    for (uint8_t i = 0; i < COAP_MESSAGE_QUEUE_SIZE; i++)
    {
        if (p_item == (coap_queue_item_t *)&m_queue[i])
        {
            memset(&m_queue[i], 0, sizeof(coap_queue_item_t));
            m_message_queue_count--;
            return NRF_SUCCESS;
        }
    }

    return (NRF_ERROR_NOT_FOUND | IOT_COAP_ERR_BASE);
}

uint32_t coap_queue_item_by_token_get(coap_queue_item_t ** pp_item, uint8_t * p_token, uint8_t token_len)
{
    for (uint8_t i = 0; i < COAP_MESSAGE_QUEUE_SIZE; i++)
    {
        if (m_queue[i].token_len == token_len)
        {
            if ((0 != m_queue[i].token_len) && 
                (memcmp(m_queue[i].token, p_token, m_queue[i].token_len) == 0))
            {
                    *pp_item = &m_queue[i];
                    return NRF_SUCCESS;
            }
        }
    }
    
    return (NRF_ERROR_NOT_FOUND | IOT_COAP_ERR_BASE);
}


uint32_t coap_queue_item_by_mid_get(coap_queue_item_t ** pp_item, uint16_t message_id)
{
    
    
    for (uint8_t i = 0; i < COAP_MESSAGE_QUEUE_SIZE; i++)
    {
        if (m_queue[i].mid == message_id)
        {
            *pp_item = &m_queue[i];
            return NRF_SUCCESS;
        }
    }
    
    return (NRF_ERROR_NOT_FOUND | IOT_COAP_ERR_BASE);
}


uint32_t coap_queue_item_next_get(coap_queue_item_t ** pp_item, coap_queue_item_t * p_item)
{
    if (p_item == NULL)
    {
        for (uint8_t i = 0; i < COAP_MESSAGE_QUEUE_SIZE; i++)
        {
            if (m_queue[i].p_buffer != NULL)
            {
                (*pp_item) = &m_queue[i];
                return NRF_SUCCESS;
            }
        }
    }
    else
    {
        uint8_t index_to_previous = (uint8_t)(((uint32_t)p_item - (uint32_t)m_queue) / (uint32_t)sizeof(coap_queue_item_t));

        for (uint8_t i = index_to_previous + 1; i < COAP_MESSAGE_QUEUE_SIZE; i++)
        {
            if (m_queue[i].p_buffer != NULL)
            {
                (*pp_item) = &m_queue[i];
                return NRF_SUCCESS;
            }
        }
    }
    (*pp_item) = NULL;

    return (NRF_ERROR_NOT_FOUND | IOT_COAP_ERR_BASE);
}
