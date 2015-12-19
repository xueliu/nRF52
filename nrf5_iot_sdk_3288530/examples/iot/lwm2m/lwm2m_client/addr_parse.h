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

#ifndef ADDR_PARSE_H__
#define ADDR_PARSE_H__

#include <stdint.h>
#include <stdbool.h>

/**@brief Parse a coap(s) uri to extract an IPv6 hostname and port if available.
 * 
 * @param[out] p_addr Array of 16 bytes to hold the parsed IPv6 address.
 * @param[out] p_port Value by reference to be filled with the parsed port number. If no port
 *                    is defined in the uri the field will be filled by CoAP default ports 
 *                    depending on wheter coap or coaps scheme was used.
 * @param[out] p_use_dtls Value by reference to indicate if it was coap or coaps scheme used.
 * @param[in]  p_uri  Pointer to the buffer containing the URI ascii characters.
 * @param[in]  uri_len Length of the p_uri buffer pointed to.
 *
 * @retval NRF_SUCCESS       If parsing of the uri was successfull.
 * @retval NRF_ERROR_NULL    If any of the supplied parameters where NULL.
 * @retval NRF_ERROR_INVALID_DATA If the function could not parse the content of p_uri.
 */
uint32_t addr_parse_uri(uint8_t *  p_addr, 
                        uint16_t * p_port, 
                        bool *     p_use_dtls, 
                        char *     p_uri, 
                        uint8_t    uri_len);

#endif // ADDR_PARSE_H__
