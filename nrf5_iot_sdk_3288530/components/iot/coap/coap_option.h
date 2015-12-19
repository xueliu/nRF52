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

/** @file coap_option.h
 *
 * @defgroup iot_sdk_coap_option CoAP Option
 * @ingroup iot_sdk_coap
 * @{
 * @brief Nordic's CoAP Option APIs.
 */

#ifndef COAP_OPTION_H__
#define COAP_OPTION_H__

/*
   +-----+---+---+---+---+----------------+--------+--------+----------+
   | No. | C | U | N | R | Name           | Format | Length | Default  |
   +-----+---+---+---+---+----------------+--------+--------+----------+
   |   1 | x |   |   | x | If-Match       | opaque | 0-8    | (none)   |
   |   3 | x | x | - |   | Uri-Host       | string | 1-255  | (see     |
   |     |   |   |   |   |                |        |        | below)   |
   |   4 |   |   |   | x | ETag           | opaque | 1-8    | (none)   |
   |   5 | x |   |   |   | If-None-Match  | empty  | 0      | (none)   |
   |   7 | x | x | - |   | Uri-Port       | uint   | 0-2    | (see     |
   |     |   |   |   |   |                |        |        | below)   |
   |   8 |   |   |   | x | Location-Path  | string | 0-255  | (none)   |
   |  11 | x | x | - | x | Uri-Path       | string | 0-255  | (none)   |
   |  12 |   |   |   |   | Content-Format | uint   | 0-2    | (none)   |
   |  14 |   | x | - |   | Max-Age        | uint   | 0-4    | 60       |
   |  15 | x | x | - | x | Uri-Query      | string | 0-255  | (none)   |
   |  17 | x |   |   |   | Accept         | uint   | 0-2    | (none)   |
   |  20 |   |   |   | x | Location-Query | string | 0-255  | (none)   |
   |  23 | x | x | - | - | Block2         | uint   | 0-3    | (none)   |
   |  27 | x | x | - | - | Block1         | uint   | 0-3    | (none)   |
   |  28 |   |   | x |   | Size2          | uint   | 0-4    | (none)   |
   |  35 | x | x | - |   | Proxy-Uri      | string | 1-1034 | (none)   |
   |  39 | x | x | - |   | Proxy-Scheme   | string | 1-255  | (none)   |
   |  60 |   |   | x |   | Size1          | uint   | 0-4    | (none)   |
   +-----+---+---+---+---+----------------+--------+--------+----------+
*/

#include <stdint.h>
#include "coap_api.h"
#include "nrf_error.h"
#include "sdk_errors.h"



typedef enum 
{
    COAP_OPT_FORMAT_EMPTY    = 0,
    COAP_OPT_FORMAT_STRING   = 1,
    COAP_OPT_FORMAT_OPAQUE   = 2,
    COAP_OPT_FORMAT_UINT     = 3
    
} coap_opt_format_t;


/**@brief Encode zero-terminated string into utf-8 encoded string.
 * 
 * @param[out]   p_encoded      Pointer to buffer that will be used to fill the 
 *                              encoded string into.
 * @param[inout] p_length       Length of the buffer provided. Will also be used to 
 *                              return the size of the used buffer.
 * @param[in]    p_string       String to encode. 
 * @param[in]    str_len        Length of the string to encode.
 *
 * @retval NRF_SUCCESS          Indicates that encoding was successful.
 * @retval NRF_ERROR_DATA_SIZE  Indicates that the buffer provided was not sufficient to 
 *                              successfully encode the data.
 */
uint32_t coap_opt_string_encode(uint8_t * p_encoded, uint16_t * p_length, uint8_t * p_string, uint16_t str_len);

/**@brief Decode a utf-8 string into a zero-terminated string.
 * 
 * @param[out]   p_string       Pointer to the string buffer where the decoded 
 *                              string will be placed.
 * @param[inout] p_length       p_length of the encoded string. Returns the size of the buffer 
 *                              used in bytes.
 * @param[in]    p_encoded      Buffer to decode.
 * 
 * @retval NRF_SUCCESS          IIndicates that decoding was successful.
 * @retval NRF_ERROR_DATA_SIZE  Indicates that the buffer provided was not sufficient to 
 *                              successfully dencode the data.
 */
uint32_t coap_opt_string_decode(uint8_t * p_string, uint16_t * p_length, uint8_t * p_encoded);

/**@brief Encode a uint value into a uint8_t buffer in network byte order.
 * 
 * @param[out]   p_encoded      Pointer to buffer that will be used to fill the 
 *                              encoded uint into.
 * @param[inout] p_length       Length of the buffer provided. Will also be used to 
 *                              return the size of the used buffer.
 * @param[in]    data           uint value which could be anything from 1 to 4 bytes.
 *
 * @retval NRF_SUCCESS          Indicates that encoding was successful.
 * @retval NRF_ERROR_DATA_SIZE  Indicates that the buffer provided was not sufficient to 
 *                              successfully encode the data.
 */
uint32_t coap_opt_uint_encode(uint8_t * p_encoded, uint16_t * p_length, uint32_t data);

/**@brief Decode a uint encoded value in network byte order to a uint32_t value.
 * 
 * @param[out]   p_data         Pointer to the uint32_t value where the decoded uint will 
 *                              be placed.
 * @param[inout] length         Size of the encoded value. 
 * @param[in]    p_encoded      uint value to be decoded into a uint32_t value.
 *
 * @retval NRF_SUCCESS              Indicates that decoding was successful.
 * @retval NRF_ERROR_NULL           If p_data or p_encoded pointer is NULL.
 * @retval NRF_ERROR_INVALID_LENGTH If buffer was greater than uint32_t. 
 */
uint32_t coap_opt_uint_decode(uint32_t * p_data, uint16_t length, uint8_t * p_encoded);

#endif // #ifndef COAP_OPTION_H__

/** @} */
