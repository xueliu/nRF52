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

/** @file ipv6_parse.h
 *
 * @defgroup iot_tools IoT Utility tools
 * @ingroup iot_sdk_common
 * @{
 * @brief Common IoT utility tools like parsing IPv6 address from a URI.
 *
 * @details This module provides utility functions like parsing IPv6 address from a URI.
 */
#ifndef IPV6_PARSE_H__
#define IPV6_PARSE_H__

#include <stdint.h>

/**
 * @brief IoT IPv6 address parsing.
 *
 * @details Supports parsing all legal IPv6 address formats as defined by the RFC.
 *
 * @param[out] p_addr  Pointer to array large enough to hold parsed address. MUST be 16 bytes big.
 * @param[in]  p_uri   String with address that should be parsed.
 * @param[in]  uri_len Length of p_uri string.
 *
 */
uint32_t ipv6_parse_addr(uint8_t * p_addr, const char * p_uri, uint8_t uri_len);

#endif
 /** @} */
