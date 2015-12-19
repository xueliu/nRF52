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
 
/** @file lwm2m_register.h
 *
 * @defgroup iot_sdk_lwm2m_register_api LWM2M register API interface
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief Register API interface for the LWM2M protocol.
 */

#ifndef LWM2M_REGISTER_H__
#define LWM2M_REGISTER_H__

#include <stdint.h>

/**@brief Initialize the LWM2M register module.
 *
 * @details Calling this function will set the module in default state.
 */
uint32_t internal_lwm2m_register_init(void);

#endif // LWM2M_REGISTER_H__

/**@} */
