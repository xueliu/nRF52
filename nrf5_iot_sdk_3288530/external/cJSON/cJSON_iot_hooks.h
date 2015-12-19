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
 
/**@file cJSON_iot_hooks.h
 *
 * @defgroup cjson cJSON parsing library - IoT hooks.
 * @{     
 *
 * @brief Memory allocation hooks using mem_manager.
 */

 
#ifndef IOT_HOOKS_H__
#define IOT_HOOKS_H__

#include <stdint.h>

/**
 * @brief Initialize cJSON with nRF's hooks. 
 *
 * @param None.
 *
 * @retval None.
 *
 */
void cJSON_Init(void);

#endif // IOT_HOOKS_H__

/** @} */
