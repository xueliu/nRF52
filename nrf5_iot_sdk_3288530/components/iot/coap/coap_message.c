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

#include "nordic_common.h"
#include "coap_message.h"
#include "coap_api.h"
#include "iot_common.h"
#include "sdk_config.h"
#include "app_util.h"

#define COAP_PAYLOAD_MARKER_SIZE  1

/**@brief Verify that there is a index available for a new option. */
#define OPTION_INDEX_AVAIL_CHECK(COUNT)                                                            \
    if ((COUNT) >= COAP_MAX_NUMBER_OF_OPTIONS)                                                     \
    {                                                                                              \
        return (NRF_ERROR_NO_MEM | IOT_COAP_ERR_BASE);                                             \
    }

#if (COAP_DISABLE_API_PARAM_CHECK == 0)

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL | IOT_COAP_ERR_BASE);                                               \
    }
#else 
    
#define NULL_PARAM_CHECK(PARAM)
#define OPTION_INDEX_AVAIL_CHECK(COUNT)

#endif // COAP_DISABLE_API_PARAM_CHECK 
 
uint32_t coap_message_create(coap_message_t * p_message, coap_message_conf_t * p_init_config)
{
    NULL_PARAM_CHECK(p_message);
    NULL_PARAM_CHECK(p_init_config);
    
    // Setting default value for version.
    p_message->header.version = COAP_VERSION;
    
    // Copy values from the init config.
    p_message->header.type       = p_init_config->type;
    p_message->header.token_len  = p_init_config->token_len;
    p_message->header.code       = p_init_config->code;
    p_message->header.id         = p_init_config->id;
    p_message->response_callback = p_init_config->response_callback;
    p_message->p_arg             = NULL;
    
    if (p_init_config->port.port_number == 0)
    {
        return (NRF_ERROR_INVALID_PARAM | IOT_COAP_ERR_BASE);
    }
    memcpy(&p_message->port, &p_init_config->port, sizeof(coap_port_t));
    
    memcpy(p_message->token, p_init_config->token, sizeof(p_init_config->token));
    return NRF_SUCCESS;
}

/**@brief Decode CoAP option
 * 
 * @param[in]    p_raw_option Pointer to the memory buffer where the raw option is located.
 * @param[inout] p_message    Pointer to the current message. Used to retrieve information about
 *                            where current option delta and the size of free memory to add the
 *                            values of the option. Used as a container where to put
 *                            the parsed option.
 * @param[out]   byte_count   Number of bytes parsed. Used to indicate where the next option 
 *                            might be located (if any left) in the raw message buffer.
 *
 * @retval NRF_SUCCESS         If the option parsing went successful.
 * @retval NRF_ERROR_DATA_SIZE If there is no more space left in the free memory to add the 
 *                             option value to the p_message.
 */
static uint32_t decode_option(const uint8_t * p_raw_option, coap_message_t * p_message, uint16_t * byte_count)
{
    uint16_t byte_index = 0;
    uint8_t  option_num = p_message->options_count;
    
    // Calculate the option number.
    uint16_t option_delta       = (p_raw_option[byte_index] & 0xF0) >> 4;
    // Calculate the option length.
    uint16_t option_length = (p_raw_option[byte_index] & 0x0F);
    byte_index++;
    
    uint16_t acc_option_delta = p_message->options_delta;
    
    if (option_delta == 13)
    {
        // read one additional byte to get the extended delta.
        acc_option_delta += 13 + p_raw_option[byte_index++];
        
    }
    else if (option_delta == 14)
    {
        // read one additional byte to get the extended delta.
        acc_option_delta += 269;
        acc_option_delta += (p_raw_option[byte_index++] << 8);
        acc_option_delta += (p_raw_option[byte_index++]);
    }
    else
    {
        acc_option_delta += option_delta;
    }
    
    // Set the accumlated delta as the option number.
    p_message->options[option_num].number = acc_option_delta;
    
    if (option_length == 13)
    {
        option_length = 13 + p_raw_option[byte_index++];
    }
    else if (option_length == 14)
    {
        option_length = 269;
        option_length += (p_raw_option[byte_index++] << 8);
        option_length += p_raw_option[byte_index++];
    }
    
    // Set the option length including extended bytes.
    p_message->options[option_num].length = option_length;
    
    // Point p_data to the memory where to find the option value.
    p_message->options[option_num].p_data = (uint8_t *)&p_raw_option[byte_index];
    
    // Update the delta counter with latest option number.
    p_message->options_delta = p_message->options[option_num].number; 
    
    byte_index += p_message->options[option_num].length;
    *byte_count = byte_index;
    
    return NRF_SUCCESS;
}

static uint32_t encode_option(uint8_t * p_buffer, coap_option_t * p_option, uint16_t * byte_count)
{
    uint16_t delta_ext      = 0;
    uint8_t  delta_ext_size = 0;
    uint8_t  delta          = p_option->number;
    
    if (p_option->number >= 13)
    {
        if (p_option->number >= 269)
        {
            delta          = 14;
            delta_ext      = p_option->number - 269;
            delta_ext_size = 2; // 2 bytes extra 
        }
        else
        {
            delta          = 13;
            delta_ext      = p_option->number - 13;
            delta_ext_size = 1; // 1 byte extra
        }     
    }   
    
    uint16_t length          = p_option->length;
    uint16_t length_ext      = 0;
    uint8_t  length_ext_size = 0;
    
    if (p_option->length >= 13)
    {
        if (p_option->length >= 269)
        {
            length          = 14;
            length_ext      = p_option->length - 269;
            length_ext_size = 2;
        }
        else
        {
            length          = 13;
            length_ext      = p_option->length - 13;
            length_ext_size = 1;
        }
    }   
    
    if (p_buffer == NULL)
    {
        uint16_t header_size = 1;
        *byte_count = header_size + delta_ext_size + length_ext_size + p_option->length;
        return NRF_SUCCESS;
    }

    uint16_t byte_index = 0;
    
    // Add the option header.
    p_buffer[byte_index++] = ((delta & 0x0F) << 4) | (length & 0x0F);
    
    // Add option delta extended bytes to the buffer.
    if (delta_ext_size == 1)
    {
        // Add first byte of delta_ext to the option header.
        p_buffer[byte_index++] = (uint8_t)delta_ext;
    }
    else if (delta_ext_size == 2)
    {
        // uint16 in Network Byte Order.
        p_buffer[byte_index++] = (uint8_t)((delta_ext & 0xFF00) >> 8);
        p_buffer[byte_index++] = (uint8_t)((delta_ext & 0x00FF));
    }
    
    if (length_ext_size == 1)
    {
        // Add first byte of length_ext to the option header.
        p_buffer[byte_index++] = (uint8_t)length_ext;
    }
    else if (length_ext_size == 2)
    {
        // uint16 in Network Byte Order.
        p_buffer[byte_index++] = (uint8_t)((length_ext & 0xFF00) >> 8);
        p_buffer[byte_index++] = (uint8_t)((length_ext & 0x00FF));
    }  
    
    memcpy(&p_buffer[byte_index], p_option->p_data, p_option->length);
    *byte_count = byte_index + p_option->length;
    
    return NRF_SUCCESS;
}

uint32_t coap_message_decode(coap_message_t *       p_message,
                             const uint8_t *        p_raw_message,
                             uint16_t               message_len)
{
    NULL_PARAM_CHECK(p_message);
    NULL_PARAM_CHECK(p_raw_message);
    
    // Check that the raw message contains the mandatory header.
    if (message_len < 4)
    {
        return (NRF_ERROR_INVALID_LENGTH | IOT_COAP_ERR_BASE);
    }
    
    // Parse the content of the raw message buffer.
    uint16_t byte_index = 0;
    
    // Parse the 4 byte CoAP header.
    p_message->header.version   = (p_raw_message[byte_index] >> 6);
    p_message->header.type      = (coap_msg_type_t)((p_raw_message[byte_index] >> 4) & 0x03);
    p_message->header.token_len = (p_raw_message[byte_index] & 0x0F);
    byte_index++;
    
    p_message->header.code      = (coap_msg_code_t)p_raw_message[byte_index];
    byte_index++;
    
    p_message->header.id        = p_raw_message[byte_index++] << 8;
    p_message->header.id       += p_raw_message[byte_index++];
    
    // Parse the token, if any.
    for (uint8_t index = 0; (byte_index < message_len) && (index < p_message->header.token_len); index++)
    {
        p_message->token[index]  = p_raw_message[byte_index++];
    }
    
    p_message->options_count = 0;
    p_message->options_delta = 0;
    
    // Parse the options if any.
    while ((byte_index < message_len) && (p_raw_message[byte_index] != COAP_PAYLOAD_MARKER))
    {

        uint32_t err_code;
        uint16_t byte_count = 0;
        
        err_code = decode_option(&p_raw_message[byte_index], p_message, &byte_count);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
        
        p_message->options_count += 1;

        byte_index += byte_count;
    }
    
    // If there any more bytes to parse this would be the payload.
    if (byte_index < message_len)
    {
        // Verify that we have a payload marker.
        if (p_raw_message[byte_index] == COAP_PAYLOAD_MARKER)
        {
            byte_index++;
        }
        else
        {
            return COAP_MESSAGE_INVALID_CONTENT;
        }
        
        p_message->payload_len = message_len - byte_index;
        p_message->p_payload = (uint8_t *)&p_raw_message[byte_index];
    }
  
    return NRF_SUCCESS;
}


uint32_t coap_message_encode(coap_message_t * p_message, 
                             uint8_t *        p_buffer, 
                             uint16_t *       p_length)
{
    NULL_PARAM_CHECK(p_length);
    NULL_PARAM_CHECK(p_message);
    
    // calculated size
    uint16_t total_packet_size = 4;
 
    if (p_message->payload_len > 0)
    {        
        total_packet_size += p_message->payload_len;
        total_packet_size += COAP_PAYLOAD_MARKER_SIZE;
    }
    
    if (p_message->header.token_len > 8)
    {
        return (NRF_ERROR_INVALID_DATA | IOT_COAP_ERR_BASE);
    }
    total_packet_size += p_message->header.token_len; 
    total_packet_size += p_message->options_len;

    // If this was a length check, return after setting the length in the output parameter.
    if (*p_length == 0)
    {
        *p_length = total_packet_size;
        return NRF_SUCCESS;
    }
 
    // Check that the buffer provided is sufficient.
    if (*p_length < total_packet_size)
    {
        return (NRF_ERROR_DATA_SIZE | IOT_COAP_ERR_BASE);
    }
    
    if (((p_message->payload_len > 0 && p_message->p_payload == NULL)) ||
        (p_buffer ==  NULL))
    {
        return COAP_MESSAGE_ERROR_NULL;
    }
    
    // Start filling the bytes.
    uint16_t byte_index = 0;
    
    // TODO: Verify the values of the header fields.
    // if (version > 1)
    // if (p_message->type > COAP_TYPE_RST)
    // if (p_message->token_len > 8)
     
    
    p_buffer[byte_index] = (((p_message->header.version & 0x3) << 6) | ((p_message->header.type & 0x3) << 4)) | (p_message->header.token_len & 0x0F);
    byte_index++;
    
    p_buffer[byte_index] = p_message->header.code;
    byte_index++;
    
    p_buffer[byte_index++] = (p_message->header.id & 0xFF00) >> 8;
    p_buffer[byte_index++] = (p_message->header.id & 0x00FF);
    
    for (uint8_t i = 0; i < p_message->header.token_len; i++)
    {
        p_buffer[byte_index++] = p_message->token[i];
    }
    
    //memcpy(&p_buffer[byte_index], &p_message->p_data[0], p_message->options_len);
    for (uint8_t i = 0; i < p_message->options_count; i++)
    {
        uint32_t err_code;
        
        uint16_t byte_count = 0;
        err_code = encode_option(&p_buffer[byte_index], &p_message->options[i], &byte_count);
        if (err_code == NRF_SUCCESS)
        {
            byte_index += byte_count;
        }
        else
        {
            // Throw an error.
        }
    }
    
    if (p_message->payload_len > 0 && p_message->p_payload != NULL)
    {
        p_buffer[byte_index++] = 0xFF;
        memcpy(&p_buffer[byte_index], p_message->p_payload, p_message->payload_len);
    }

    *p_length = total_packet_size;
    
    return NRF_SUCCESS;
}

uint32_t coap_message_opt_empty_add(coap_message_t * p_message, uint16_t option_num)
{
    OPTION_INDEX_AVAIL_CHECK(p_message->options_count);

    uint32_t err_code;
    uint16_t encoded_len          = 0;
    uint8_t  current_option_index = p_message->options_count;

    p_message->options[current_option_index].number = option_num - p_message->options_delta;
    p_message->options[current_option_index].length = encoded_len;

    // Set accumulated option delta for next option.
    p_message->options_delta = option_num;
    
    // Calculate option size
    uint16_t option_byte_count = 0;
    
    // do a length check to encode_option to get the header length.
    err_code = encode_option(NULL, &p_message->options[current_option_index], &option_byte_count);
    
    // Accumulate expected size of all options with headers.
    p_message->options_len    += option_byte_count;

    p_message->options_count  += 1;

    return err_code;
}

uint32_t coap_message_opt_uint_add(coap_message_t * p_message, 
                                   uint16_t         option_num, 
                                   uint32_t         data)
{
    OPTION_INDEX_AVAIL_CHECK(p_message->options_count);

    uint32_t err_code;
    uint16_t encoded_len          = p_message->data_len - p_message->options_offset;
    uint8_t  current_option_index = p_message->options_count;
    uint8_t * p_next_option_data  = &p_message->p_data[p_message->options_offset];
    
    // If the value of the option is 0, do not encode the 0, as this can be omitted. (RFC7252 3.2)
    if (data == 0)
    {
        encoded_len = 0;
    }
    else
    {
        err_code = coap_opt_uint_encode(p_next_option_data, &encoded_len, data);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    
    p_message->options[current_option_index].p_data = p_next_option_data;
    p_message->options[current_option_index].number = option_num - p_message->options_delta;
    p_message->options[current_option_index].length = encoded_len;

    // Set accumulated option delta for next option.
    p_message->options_delta = option_num;
    
    // Calculate option size
    uint16_t option_byte_count = 0;
    
    // do a length check to encode_option to get the header length.
    err_code = encode_option(NULL, &p_message->options[current_option_index], &option_byte_count);
    
    // Accumulate expected size of all options with headers.
    p_message->options_len    += option_byte_count;
    
    p_message->options_count  += 1;
    p_message->options_offset += encoded_len;
    
    return err_code;
}

uint32_t coap_message_opt_str_add(coap_message_t * p_message, uint16_t option_num, uint8_t * p_data, uint16_t length)
{
    OPTION_INDEX_AVAIL_CHECK(p_message->options_count);

    uint32_t err_code;

    uint16_t encoded_len          = length;
    uint8_t  current_option_index = p_message->options_count;
    uint8_t * p_next_option_data  = &p_message->p_data[p_message->options_offset];
    

    err_code = coap_opt_string_encode(p_next_option_data, &encoded_len, p_data, length);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    p_message->options[current_option_index].p_data = p_next_option_data;
    p_message->options[current_option_index].number = option_num - p_message->options_delta;
    p_message->options[current_option_index].length = encoded_len;

    // Set accumulated option delta for next option.
    p_message->options_delta = option_num;
    
    // Calculate option size
    uint16_t option_byte_count = 0;
    
    // do a length check to encode_option to get the header length.
    err_code = encode_option(NULL, &p_message->options[current_option_index], &option_byte_count);
    
    // Accumulate expected size of all options with headers.
    p_message->options_len    += option_byte_count;
    
    p_message->options_count  += 1;
    p_message->options_offset += encoded_len;    

    return err_code;   

}

uint32_t coap_message_opt_opaque_add(coap_message_t * p_message, uint16_t option_num, uint8_t * p_data, uint16_t length)
{
    OPTION_INDEX_AVAIL_CHECK(p_message->options_count);

    // Check if it is possible to add a new option of this length.
    if ((p_message->data_len - p_message->options_offset) < length)
    {
        return (NRF_ERROR_DATA_SIZE | IOT_COAP_ERR_BASE);
    }

    uint32_t err_code = NRF_SUCCESS;

    uint16_t  encoded_len          = length;
    uint8_t   current_option_index = p_message->options_count;
    uint8_t * p_next_option_data   = &p_message->p_data[p_message->options_offset];
    

    memcpy(p_next_option_data, p_data, encoded_len);

    p_message->options[current_option_index].p_data = p_next_option_data;
    p_message->options[current_option_index].number = option_num - p_message->options_delta;
    p_message->options[current_option_index].length = encoded_len;

    // Set accumulated option delta for next option.
    p_message->options_delta = option_num;
    
    // Calculate option size
    uint16_t option_byte_count = 0;
    
    // do a length check to encode_option to get the header length.
    err_code = encode_option(NULL, &p_message->options[current_option_index], &option_byte_count);
    
    // Accumulate expected size of all options with headers.
    p_message->options_len    += option_byte_count;
    
    p_message->options_count  += 1;
    p_message->options_offset += encoded_len;    

    return err_code;  
}

uint32_t coap_message_payload_set(coap_message_t * p_message, 
                                  void *           p_payload, 
                                  uint16_t         payload_len)
{
    p_message->p_payload = &p_message->p_data[p_message->options_offset];
    p_message->payload_len = payload_len;
    memcpy(p_message->p_payload, p_payload, payload_len);
    
    return NRF_SUCCESS;
}


uint32_t coap_message_remote_addr_set(coap_message_t * p_message, coap_remote_t * p_address)
{
    memcpy(&p_message->remote, p_address, sizeof(coap_remote_t));
    return NRF_SUCCESS;
}

uint32_t coap_message_opt_index_get(uint8_t * p_index, coap_message_t * p_message, uint16_t option)
{
    NULL_PARAM_CHECK(p_index);
    NULL_PARAM_CHECK(p_message);

    uint8_t index;
    for (index = 0; index < p_message->options_count; index++)
    {
        if (p_message->options[index].number == option)
        {
            *p_index = index;
            return NRF_SUCCESS;
        }
    } 
    return (NRF_ERROR_NOT_FOUND | IOT_COAP_ERR_BASE);
}

uint32_t coap_message_opt_present(coap_message_t * p_message, uint16_t option)
{
    NULL_PARAM_CHECK(p_message);

    uint8_t index;
    for (index = 0; index < p_message->options_count; index++)
    {
        if (p_message->options[index].number == option)
        {
            return NRF_SUCCESS;
        }
    } 
    return (NRF_ERROR_NOT_FOUND | IOT_COAP_ERR_BASE);
}

static uint32_t bit_to_content_format(coap_content_type_t * p_ct, uint32_t bit)
{
    switch (bit)
    {
        case COAP_CT_MASK_PLAIN_TEXT:
            *p_ct = COAP_CT_PLAIN_TEXT; 
            break;
        
        case COAP_CT_MASK_APP_LINK_FORMAT:
            *p_ct = COAP_CT_APP_LINK_FORMAT;
            break;
        
        case COAP_CT_MASK_APP_XML:
            *p_ct = COAP_CT_APP_XML;
            break;
        
        case COAP_CT_MASK_APP_OCTET_STREAM:
            *p_ct = COAP_CT_APP_OCTET_STREAM;
            break;
        
        case COAP_CT_MASK_APP_EXI:
            *p_ct = COAP_CT_APP_EXI;
            break;
        
        case COAP_CT_MASK_APP_JSON:
            *p_ct = COAP_CT_APP_JSON;
            break;
        
        default:
            return (NRF_ERROR_NOT_FOUND | IOT_COAP_ERR_BASE);
    }
    return NRF_SUCCESS;
}

static uint32_t content_format_to_bit(coap_content_type_t ct)
{
    uint32_t mask = 0;
    switch (ct)
    {
        case COAP_CT_PLAIN_TEXT:
            mask = COAP_CT_MASK_PLAIN_TEXT; 
            break;
        
        case COAP_CT_APP_LINK_FORMAT:
            mask = COAP_CT_MASK_APP_LINK_FORMAT;
            break;
        
        case COAP_CT_APP_XML:
            mask = COAP_CT_MASK_APP_XML;
            break;
        
        case COAP_CT_APP_OCTET_STREAM:
            mask = COAP_CT_MASK_APP_OCTET_STREAM;
            break;
        
        case COAP_CT_APP_EXI:
            mask = COAP_CT_MASK_APP_EXI;
            break;
        
        case COAP_CT_APP_JSON:
            mask = COAP_CT_MASK_APP_JSON;
            break;
        
        default:
            break;
    }
        
    return mask;
}

uint32_t coap_message_ct_mask_get(coap_message_t * p_message, uint32_t * p_mask)
{
    NULL_PARAM_CHECK(p_message);
    NULL_PARAM_CHECK(p_mask);

    (*p_mask) = 0;
    
    for (uint8_t index = 0; index < p_message->options_count; index++)
    {
        if (p_message->options[index].number == COAP_OPT_CONTENT_FORMAT)
        {    
            uint32_t value;
            uint32_t err_code = coap_opt_uint_decode(&value, 
                                                     p_message->options[index].length, 
                                                     p_message->options[index].p_data);
            if (err_code == NRF_SUCCESS)
            {
                coap_content_type_t ct = (coap_content_type_t)value;
                *p_mask |= content_format_to_bit(ct);
            }
            else
            {
                return err_code;
            }
        }
    }
    return NRF_SUCCESS;
}

uint32_t coap_message_accept_mask_get(coap_message_t * p_message, uint32_t * p_mask)
{
    NULL_PARAM_CHECK(p_message);
    NULL_PARAM_CHECK(p_mask);
    
    (*p_mask) = 0;
    
    for (uint8_t index = 0; index < p_message->options_count; index++)
    {
        if (p_message->options[index].number == COAP_OPT_ACCEPT)
        {    
            uint32_t value;
            uint32_t err_code = coap_opt_uint_decode(&value, 
                                                     p_message->options[index].length, 
                                                     p_message->options[index].p_data);
            if (err_code == NRF_SUCCESS)
            {
                coap_content_type_t ct = (coap_content_type_t)value;
                (*p_mask) |= content_format_to_bit(ct);
            }
            else
            {
                return err_code;
            }
        }
    }
    
    return NRF_SUCCESS;
}

uint32_t coap_message_ct_match_select(coap_content_type_t * p_ct, coap_message_t * p_message, coap_resource_t * p_resource)
{
    // Check ACCEPT options
    uint32_t accept_mask = 0;
    (void)coap_message_accept_mask_get(p_message, &accept_mask);
    
    if (accept_mask == 0)
    {
        // Default to plain text if option not set.
        accept_mask = COAP_CT_MASK_PLAIN_TEXT;
    }
    
    // Select the first common content-type between the resource and the CoAP client.
    uint32_t common_ct            = p_resource->ct_support_mask & accept_mask;
    uint32_t bit_index;
    for (bit_index = 0; bit_index < 32; bit_index++)
    {
        if (((common_ct >> bit_index) & 0x1 ) == 1)
        {
            break;
        }
    }
    
    uint32_t err_code = bit_to_content_format(p_ct, 1 << bit_index);

    return err_code;
}
