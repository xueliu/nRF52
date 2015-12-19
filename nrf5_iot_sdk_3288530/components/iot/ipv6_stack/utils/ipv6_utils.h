/* Copyright (c)  2014 Nordic Semiconductor. All Rights Reserved.
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

/** @file ipv6_utils.h
 *
 * @defgroup iot_utils Common utils
 * @ingroup iot_sdk_common
 * @{
 * @brief Abstracts common IOT macros needed by IOT modules.
 *
 * @details Common macros related to IOT are defined here for use by all IOT modules.
 */

#ifndef IPV6_UTILS_H__
#define IPV6_UTILS_H__

#include <stdint.h>
#include <stdbool.h>
#include "iot_defines.h"

/**@brief Function for calculating checksum using IPv6 algorithm.
 *
 * @param[in]  p_data      Pointer to the data needs to be checksummed.
 * @param[in]  len         Length of the data.
 * @param[in]  p_checksum  Pointer to actual value of checksum.
 * @param[out] p_checksum  Value of calculated checksum.
 *
 * @retval None.
 */
void ipv6_checksum_calculate(const uint8_t * p_data,
                             uint16_t        len,
                             uint16_t      * p_checksum,
                             bool            flip_zero);


/**@brief Function for initializing default values of IPv6 Header.
 *
 * @note  Function initializes Version, Traffic Class, Flow Label, Next Header, Hop Limit and 
 *        Length fields.
 *
 * @param[in] p_ip_header Pointer to the IPv6 Header.
 *
 * @retval None.
 */
void ipv6_header_init(ipv6_header_t * p_ip_header);

#endif //IPV6_UTILS_H__

/**@} */
