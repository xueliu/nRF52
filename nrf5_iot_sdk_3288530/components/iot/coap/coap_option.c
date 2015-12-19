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

#include <stdlib.h>
#include <string.h>

#include "coap_option.h"
#include "iot_common.h"

#if (COAP_DISABLE_API_PARAM_CHECK == 0)

#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL | IOT_COAP_ERR_BASE);                                               \
    }
    
#else
    
#define NULL_PARAM_CHECK(PARAM)

#endif // COAP_DISABLE_API_PARAM_CHECK 

uint32_t coap_opt_string_encode(uint8_t * p_encoded, uint16_t * p_length, uint8_t * p_string, uint16_t str_len)
{
    NULL_PARAM_CHECK(p_encoded);
    NULL_PARAM_CHECK(p_length);
    NULL_PARAM_CHECK(p_string);
    
    if (str_len > *p_length)
    {
        return (NRF_ERROR_DATA_SIZE | IOT_COAP_ERR_BASE);
    }
    
    memcpy(p_encoded, p_string, str_len);
    
    *p_length = str_len;
    
    return NRF_SUCCESS;
}

uint32_t coap_opt_string_decode(uint8_t * p_string, uint16_t * p_length, uint8_t * p_encoded)
{
    return NRF_SUCCESS;
}

uint32_t coap_opt_uint_encode(uint8_t * p_encoded, uint16_t * p_length, uint32_t data)
{
    NULL_PARAM_CHECK(p_encoded);
    NULL_PARAM_CHECK(p_length);
    
    uint16_t byte_index = 0;
    
    if (data <= UINT8_MAX)
    {
        if (*p_length < sizeof(uint8_t))
        {
            return (NRF_ERROR_DATA_SIZE | IOT_COAP_ERR_BASE);
        }
        
        p_encoded[byte_index++] = (uint8_t)data;
    }
    else if (data <= UINT16_MAX)
    {
        if (*p_length < sizeof(uint16_t))
        {
            return (NRF_ERROR_DATA_SIZE | IOT_COAP_ERR_BASE);
        }
        
        p_encoded[byte_index++] = (uint8_t)((data & 0xFF00) >> 8);
        p_encoded[byte_index++] = (uint8_t)(data & 0x00FF);
    }
    else
    {
        if (*p_length < sizeof(uint32_t))
        {
            return (NRF_ERROR_DATA_SIZE | IOT_COAP_ERR_BASE);
        }
        
        p_encoded[byte_index++] = (uint8_t)((data & 0xFF000000) >> 24);
        p_encoded[byte_index++] = (uint8_t)((data & 0x00FF0000) >> 16);
        p_encoded[byte_index++] = (uint8_t)((data & 0x0000FF00) >> 8);
        p_encoded[byte_index++] = (uint8_t)(data & 0x000000FF);
    }
    
    
    
    *p_length = byte_index;
    
    return NRF_SUCCESS;
}

uint32_t coap_opt_uint_decode(uint32_t * p_data, uint16_t length, uint8_t * p_encoded)
{
    NULL_PARAM_CHECK(p_data);
    NULL_PARAM_CHECK(p_encoded);
    
    uint8_t byte_index = 0;
    switch (length)
    {
        case 0:
            {
                *p_data = 0;
            }
            break;
            
        case 1:
            {
                *p_data = 0;
                *p_data |= p_encoded[byte_index++];
            }
            break;

        case 2:
            {
                *p_data = 0;
                *p_data |= (p_encoded[byte_index++] << 8);
                *p_data |= (p_encoded[byte_index++]);
            }
            break;

        case 3:
            {
                *p_data = 0;
                *p_data |= (p_encoded[byte_index++] << 16);
                *p_data |= (p_encoded[byte_index++] << 8);
                *p_data |= (p_encoded[byte_index++]);
            }
            break;

        case 4:
            {
                *p_data = 0;
                *p_data |= (p_encoded[byte_index++] << 24);
                *p_data |= (p_encoded[byte_index++] << 16);
                *p_data |= (p_encoded[byte_index++] << 8);
                *p_data |= (p_encoded[byte_index++]);
            }
            break;
        
        default:
            return (NRF_ERROR_INVALID_LENGTH | IOT_COAP_ERR_BASE);
    }

    return NRF_SUCCESS;
}
