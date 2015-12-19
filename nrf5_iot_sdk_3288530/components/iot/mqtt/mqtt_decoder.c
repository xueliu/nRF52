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

/** @file mqtt_decoder.c
 *
 * @brief Decoder functions needs for deconding packets received from the broker.
 */

#include "mqtt_internal.h"


uint32_t unpack_uint8(uint8_t    * p_val,
                      uint32_t     buffer_len,
                      uint8_t    * const buffer,
                      uint32_t   * const p_offset)
{
    uint32_t       err_code = NRF_ERROR_DATA_SIZE;

    if (buffer_len > (*p_offset))
    {
        const uint32_t available_len = buffer_len - (*p_offset);

        MQTT_TRC("[MQTT]: >> unpack_uint8 BL:%08x, B:%p, O:%08x A:%08x\r\n",
                 buffer_len, buffer, (*p_offset), available_len);

        if (available_len >= SIZE_OF_UINT8)
        {
            // Create unit8 value.
            (*p_val)     = buffer[*p_offset];

            // Increment offset.
            (*p_offset) += SIZE_OF_UINT8;

            // Indicate success.
            err_code     = NRF_SUCCESS;
        }
    }

    MQTT_TRC("[MQTT]: >> unpack_uint8 result:0x%08x val:0x%02x\r\n",err_code, (*p_val));
    return err_code;
}


uint32_t unpack_uint16(uint16_t   * p_val,
                       uint32_t     buffer_len,
                       uint8_t    * const buffer,
                       uint32_t   * const p_offset)
{
    uint32_t       err_code = NRF_ERROR_DATA_SIZE;

    if (buffer_len > (*p_offset))
    {
        const uint32_t available_len = buffer_len - (*p_offset);

        MQTT_TRC("[MQTT]: << unpack_uint16 BL:%08x, B:%p, O:%08x A:%08x\r\n",
                 buffer_len, buffer, (*p_offset), available_len);

        if (available_len >= SIZE_OF_UINT16)
        {
            // Create unit16 value.
            (*p_val)     = ((buffer[*p_offset]    & 0x00FF) << 8); // MSB
            (*p_val)    |= (buffer[(*p_offset+1)] & 0x00FF);        // LSB

            // Increment offset.
            (*p_offset) += SIZE_OF_UINT16;

            // Indicate success.
            err_code     = NRF_SUCCESS;
        }
    }

    MQTT_TRC("[MQTT]: >> unpack_uint16 result:0x%08x val:0x%04x\r\n",err_code, (*p_val));

    return err_code;
}


uint32_t unpack_utf8_str(mqtt_utf8_t    * const p_str,
                         uint32_t               buffer_len,
                         uint8_t        * const buffer,
                         uint32_t       * const p_offset)
{
    uint16_t utf8_strlen;
    uint32_t err_code = unpack_uint16(&utf8_strlen, buffer_len, buffer, p_offset);

    p_str->p_utf_str  = NULL;
    p_str->utf_strlen = 0;

    if (err_code == NRF_SUCCESS)
    {
        const uint32_t available_len = buffer_len - (*p_offset);

        MQTT_TRC("[MQTT]: << unpack_utf8_str BL:%08x, B:%p, O:%08x A:%08x\r\n",
                 buffer_len, buffer, (*p_offset), available_len);

        if (utf8_strlen <= available_len)
        {
            // Zero length UTF8 strings permitted.
            if (utf8_strlen)
            {
                // Point to right location in buffer.
                p_str->p_utf_str   = &buffer[(*p_offset)];
            }

            // populate length field.
            p_str->utf_strlen  = utf8_strlen;

            // Increment offset.
            (*p_offset)       += utf8_strlen;

            // Indicate success
            err_code = NRF_SUCCESS;
        }
        else
        {
            // Reset to original value.
            (*p_offset) -= SIZE_OF_UINT16;

            err_code = NRF_ERROR_DATA_SIZE;
        }
    }

    MQTT_TRC("[MQTT]: >> unpack_utf8_str result:0x%08x utf8 len:0x%08x\r\n",err_code, p_str->utf_strlen);

    return err_code;
}


uint32_t unpack_bin_str(mqtt_binstr_t    * const p_str,
                        uint32_t                 buffer_len,
                        uint8_t          * const buffer,
                        uint32_t         * const p_offset)
{
    uint32_t error_code = NRF_ERROR_DATA_SIZE;

    MQTT_TRC("[MQTT]: << unpack_bin_str BL:%08x, B:%p, O:%08x \r\n",
             buffer_len, buffer, (*p_offset));

    if (buffer_len >= (*p_offset))
    {
        p_str->p_bin_str  = NULL;
        p_str->bin_strlen = 0;

        // Indicate success zero length binary strings are permitted.
        error_code        = NRF_SUCCESS;

        const uint32_t available_len = buffer_len - (*p_offset);

        if (available_len)
        {
            // Point to start of binary string.
            p_str->p_bin_str  = &buffer[(*p_offset)];
            p_str->bin_strlen = available_len;

            // Increment offset.
            (*p_offset)      += available_len;
        }
    }

    MQTT_TRC("[MQTT]: >> unpack_bin_str bin len:0x%08x\r\n", p_str->bin_strlen);

    return error_code;
}


void packet_length_decode(uint8_t * p_buff, uint32_t * p_remaining_length, uint32_t * p_size)
{
    uint32_t index            = (*p_size);
    uint32_t remaining_length = 0;
    uint32_t multiplier       = 1;

    do
    {
        remaining_length += (p_buff[index] & 0x7F) * multiplier;
        multiplier       *= 0x80;

    } while ((p_buff[index++] & 0x80) != 0);

    *p_size             = index;
    *p_remaining_length = remaining_length;

    MQTT_TRC("[MQTT]: packet_length_decode: RL:0x%08x RLS:0x%08x\r\n", remaining_length, index);
}
