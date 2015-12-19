/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 */

/** @file
 *
 * @defgroup ipv6_medium_ble BLE IPv6 Medium Implementation
 * @{
 * @ingroup iot_sdk_common
 * @brief Bluetooth Low Energy implementation of the IPv6 medium interface.
 *
 * @details Type definitions for the BLE implementation of the IPv6 medium interface. 
 *          This header also includes the header with BLE-specific configuration. 
 */

#ifndef IPV6_MEDIUM_BLE_H__
#define IPV6_MEDIUM_BLE_H__

#include <stdint.h>
#include "ble.h"
#include "ble_advdata.h"
#include "ipv6_medium_ble_cfg.h"

/**@brief Structure for storing all GAP parameters. */
typedef struct
{
    uint16_t                  appearance;
    uint8_t const           * p_dev_name; 
    uint16_t                  dev_name_len;
    ble_gap_conn_sec_mode_t   sec_mode;
    ble_gap_conn_params_t     gap_conn_params;
} ipv6_medium_ble_gap_params_t;

/**@brief Structure for storing all advertisement parameters. */
typedef struct
{
    ble_advdata_t            advdata;
    ble_advdata_manuf_data_t adv_man_specific_data;
    ble_advdata_t            srdata;
    ble_advdata_manuf_data_t sr_man_specific_data;
    ble_gap_adv_params_t     advparams;
} ipv6_medium_ble_adv_params_t;

/**@brief Structure of BLE-specific parameters of events passed to the parent layer by the IPv6 medium. */
typedef struct
{
    ble_evt_t * p_ble_evt;
} ipv6_medium_ble_cb_params_t;

/**@brief Structure of BLE-specific parameters of errors passed to the parent layer by the IPv6 medium. */
typedef struct
{
    uint8_t dummy_value; // Parameters to be added.
} ipv6_medium_ble_error_params_t;

#endif // IPV6_MEDIUM_BLE_H__

/** @} */
