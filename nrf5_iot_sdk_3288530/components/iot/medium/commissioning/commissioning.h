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
 * @defgroup commissioning_module Commissioning Module
 * @{
 * @ingroup iot_sdk_common
 * @brief Commissioning module.
 *
 * @details Enables commissioning of the node by managing transitions between the Config, Joining, and
 *          Identity modes. In Config mode the node can be configured with the settings required to 
 *          join the network in Joining mode. The Identity mode can be requested to make the node 
 *          easily recognizeable for the user. 
 *          The settings managed by the module are stored in persistent storage.
 *
 */

#ifndef COMMISSIONING_H__
#define COMMISSIONING_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble_ncfgs.h"
#include "pstorage.h"
#include "iot_timer.h"
#include "ipv6_medium_ble.h"

#define COMPANY_IDENTIFIER                          0x0059                                          /**< Company identifier for Nordic Semiconductor ASA as per www.bluetooth.org. */

#define COMMISSIONING_TICK_INTERVAL_SEC             1                                               /**< Interval between periodic callbacks to the Commissioning module. */

#define COMMISSIONING_EVT_CONFIG_MODE_ENTER         0x01                                            /**< Indicates that the medium entered mode for commissioning configuration. */
#define COMMISSIONING_EVT_JOINING_MODE_ENTER        0x02                                            /**< Indicates that the medium exited mode for commissioning configuration. */
#define COMMISSIONING_EVT_IDENTITY_MODE_ENTER       0x03                                            /**< Indicates that identity mode was requested. */
#define COMMISSIONING_EVT_IDENTITY_MODE_EXIT        0x04                                            /**< Indicates that the node should stop using any features associated with the Identity mode. */

#define NODE_MODE_NONE                              0x00                                            /**< Node mode: before initialization. */
#define NODE_MODE_JOINING                           0x01                                            /**< Node mode: joining the network. */
#define NODE_MODE_CONFIG                            0x02                                            /**< Node mode: configuration. */
#define NODE_MODE_IDENTITY                          0x03                                            /**< Node mode: conspicuous for the user. */

/**@brief Joining mode timer control commands. */
typedef enum 
{
    JOINING_MODE_TIMER_START      = 0x01, 
    JOINING_MODE_TIMER_STOP_RESET = 0x02
} joining_mode_timer_ctrl_cmd_t;

/**@brief Structure for storing all settings necessary for commissioning. */
typedef struct __attribute__ ((__packed__)) __attribute__((aligned))
{
    uint8_t             poweron_mode;          // Checked at startup to enter correct mode.
    state_on_failure_t  joining_mode_failure;  // Mode to enter if Joining mode fails.
    uint32_t            joining_mode_to;       // General timeout in Joining mode.
    state_on_failure_t  config_mode_failure;   // Mode to enter if Config mode fails.
    uint32_t            config_mode_to;        // General timeout in Config mode.
    uint32_t            id_mode_to;            // Duration of Identity Mode.
    ssid_store_t        ssid_store;            // SSID received from the router.
    keys_store_t        keys_store;            // Keys received from the router.
    id_data_store_t     id_data_store;         // Custom node identifier data.
} commissioning_settings_t;

/**@brief Commissioning module event handler type. */
typedef void (*commissioning_timeout_handler_t)(void);

/**@brief Structure for creating timers in the Commissioning module. */
typedef struct
{
    bool                            is_timer_running;
    uint32_t                        current_value_sec;
    commissioning_timeout_handler_t timeout_handler;
} commissioning_timer_t;

/**@brief Structure of events passed by the Commissioning module to the parent layer. */
typedef struct
{
    uint8_t                    commissioning_evt_id;
    bool                       power_off_enable_requested;
    commissioning_settings_t * p_commissioning_settings;
} commissioning_evt_t;

/**@brief Function for handling BLE events.
 *
 * @details This function must be called from the BLE stack event dispatcher 
 *          to handle BLE events that are relevant for the Commissioning module.
 *
 * @param[in] p_ble_evt BLE stack event.
 */
void commissioning_ble_evt_handler(const ble_evt_t * p_ble_evt);

/**@brief Commissioning module event handler type. */
typedef void (*commissioning_evt_handler_t)(commissioning_evt_t * p_commissioning_evt);

/**@brief Structure for initialization parameters of the Commissioning module. */
typedef struct
{
    commissioning_evt_handler_t commissioning_evt_handler;
} commissioning_init_params_t;

/**@brief Function for initializing the Commissioning module. 
 *
 * @details Initializes the Node Configuration Service module to create the GATT database.
 *          Loads previously stored node settings from the persistent storage and if 
 *          the settings are valid, sets up the node to start in the right mode.
 *
 * @param[in]    p_init_param      Pointer to the initialization parameters.
 * @param[out]   p_poweron_state   Pointer to the value of the mode that should be started.
 *
 * @retval       NRF_SUCCESS       If initialization was successful. Otherwise, a propagated 
 *                                 error code is returned.
 *
 */
uint32_t commissioning_init(commissioning_init_params_t * p_init_param, \
                            uint8_t                     * p_poweron_state);

/**@brief Function for advancing the node to a new mode. 
 *
 * @details Stops and starts app timers appropriate for the mode requested. 
 *          Propagates the mode change event to the parent layer.
 *
 * @param[in]    new_mode    New mode to start.
 *
 */
void commissioning_node_mode_change(uint8_t new_mode);

/**@brief Function for getting the address of GAP parameters for the active mode. 
 *
 * @param[out]   pp_node_gap_params    Address of GAP parameters for the active mode.
 *
 */
void commissioning_gap_params_get(ipv6_medium_ble_gap_params_t ** pp_node_gap_params);

/**@brief Function for getting the address of advertising parameters for the active mode. 
 *
 * @param[out]   pp_node_adv_params    Address of advertising parameters for the active mode.
 *
 */
void commissioning_adv_params_get(ipv6_medium_ble_adv_params_t ** pp_node_adv_params);

/**@brief Function for clearing all node settings from the persistent storage. 
 *
 * @details Calls the appropriate persistent storage interface function to clear
 *          all commissioning-related settings from the persistent storage.
 *
 */
void commissioning_settings_clear(void);

/**@brief Function for controlling the joining mode timer from the parent layer(s). 
 *
 * @details If the Joining mode timer reaches zero, the node must enter the 
 *          state-on-failure, as set by the user. This funtion allows the 
 *          application designer to control the Joining mode timer from the
 *          application layer.
 *
 */
void commissioning_joining_mode_timer_ctrl( \
         joining_mode_timer_ctrl_cmd_t joining_mode_timer_ctrl_cmd);

/**@brief Commissioning time tick used for measuring delays and time between events.
 * 
 * @param[in]    wall_clock_value    Wall clock value from the IoT Timer module.
 */
void commissioning_time_tick(iot_timer_time_in_ms_t wall_clock_value);

#endif // COMMISSIONING_H__

/** @} */
