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
 * @defgroup ipv6_medium IPv6 Medium
 * @{
 * @ingroup iot_sdk_common
 * @brief IPv6 Medium Interface.
 *
 * @details Implementation-agnostic interface of the physical transport that 
 *          facilitates IPv6 traffic.
 */

#ifndef IPV6_MEDIUM_H__
#define IPV6_MEDIUM_H__

#include <stdint.h>
#include "ipv6_medium_platform.h"
#include "iot_defines.h"

#define EUI_48_SIZE                               6                                                 /**< Size of a 48-bit Extended Unique Identifier in bytes. */

#define IPV6_MEDIUM_ID_ANY                        0x00                                              /**< Indicates invalid physical transport type. */
#define IPV6_MEDIUM_ID_BLE                        0x01                                              /**< Indicates that the physical transport is BLE. */
#define IPV6_MEDIUM_ID_802154                     0x02                                              /**< Indicates that the physical transport is 802.15.4. */

#define IPV6_MEDIUM_EVT_CONN_DOWN                 0x01                                              /**< Indicates that a connection is established. */
#define IPV6_MEDIUM_EVT_CONN_UP                   0x02                                              /**< Indicates that a connection is torn down. */
#define IPV6_MEDIUM_EVT_CONNECTABLE_MODE_ENTER    0x01                                              /**< Indicates that the medium entered connectable mode. */
#define IPV6_MEDIUM_EVT_CONNECTABLE_MODE_EXIT     0x02                                              /**< Indicates that the medium exited connectable mode. */
#define IPV6_MEDIUM_EVT_MAC_ADDRESS_CHANGED       0x03                                              /**< Indicates that the device has a new MAC address. */
#define IPV6_MEDIUM_EVT_PHY_SPECIFIC              0xFF                                              /**< Indicates miscellaneous event from the physical layer. */

/**@brief IPv6 medium instance identifier type. */
typedef uint32_t ipv6_medium_instance_id_t;

/**@brief Type of IPv6 medium type. */
typedef uint8_t ipv6_medium_type_t;

/**@brief IPv6 medium instance type. */
typedef struct
{
    ipv6_medium_instance_id_t ipv6_medium_instance_id;
    ipv6_medium_type_t        ipv6_medium_instance_type;
} ipv6_medium_instance_t;

/**@brief EUI-48 value type. */
typedef struct
{
    uint8_t identifier[EUI_48_SIZE];  /**< 48-bit identifier. */
} eui48_t;

/**@brief  Type of IPv6 medium event parameters. */
typedef struct
{
    ipv6_medium_instance_t        ipv6_medium_instance_id;
    uint8_t                       ipv6_medium_evt_id;
    ipv6_medium_cb_params_union_t medium_specific;
} ipv6_medium_evt_t;

/**@brief Type of IPv6 medium error parameters. */
typedef struct
{
    ipv6_medium_instance_t         ipv6_medium_instance_id;
    uint32_t                       error_label;
    ipv6_medium_err_params_union_t medium_specific;
} ipv6_medium_error_t;

/**@brief IPv6 medium event handler type. */
typedef void (*ipv6_medium_evt_handler_t)(ipv6_medium_evt_t * p_ipv6_medium_evt);

/**@brief IPv6 medium error handler type. */
typedef void (*ipv6_medium_error_handler_t)(ipv6_medium_error_t * p_ipv6_medium_error);

#ifdef COMMISSIONING_ENABLED
/**@brief Commissioning mode control commands. */
typedef enum 
{
    CMD_IDENTITY_MODE_EXIT   = 0x00, 
    CMD_IDENTITY_MODE_ENTER  = 0x01
} mode_control_cmd_t;

/**@brief Commissioning: Identity mode control callback funtion type. */
typedef void (*commissioning_id_mode_cb_t)(mode_control_cmd_t control_command);

/**@brief Commissioning: Power off on failure control callback funtion type. */
typedef void (*commissioning_poweroff_cb_t)(bool do_poweroff_on_failure);
#endif // COMMISSIONING_ENABLED

/**@brief Structure for initialization parameters of the IPv6 medium. */
typedef struct 
{
    ipv6_medium_evt_handler_t    ipv6_medium_evt_handler;
    ipv6_medium_error_handler_t  ipv6_medium_error_handler;
    bool                         use_scheduler;
#ifdef COMMISSIONING_ENABLED
    commissioning_id_mode_cb_t   commissioning_id_mode_cb;
    commissioning_poweroff_cb_t  commissioning_power_off_cb;
#endif // COMMISSIONING_ENABLED
} ipv6_medium_init_params_t;

/**@brief Function for initializing the IPv6 medium. 
 *
 * @details Initializes the IPv6 medium module. 
 *          Performs all setup necessary that is specific to the implementation.
 *
 * @param[in]    p_init_param            Pointer to the initialization parameters.
 * @param[in]    desired_medium_type     Value of the desired medium type.
 * @param[out]   p_new_medium_instance   Pointer to the new medium instance initalized.
 *
 * @retval       NRF_SUCCESS    If initialization was successful. Otherwise, a propagated 
 *                              error code is returned.
 *
 */
uint32_t ipv6_medium_init(ipv6_medium_init_params_t * p_init_param,           \
                          ipv6_medium_type_t          desired_medium_type,    \
                          ipv6_medium_instance_t    * p_new_medium_instance);

/**@brief Function for entering connectible mode.
 *
 * @details Requests the IPv6 medium to enter connectible mode.
 *
 * @param[in]    ipv6_medium_instance_id    Specifies the IPv6 medium instance.
 *
 * @retval       NRF_SUCCESS    If the procedure was successful. Otherwise, a propagated 
 *                              error code is returned.
 *
 */
uint32_t ipv6_medium_connectable_mode_enter(ipv6_medium_instance_id_t ipv6_medium_instance_id);

/**@brief Function for exiting connectible mode.
 *
 * @details Requests the IPv6 medium to exit connectible mode.
 *
 * @param[in]    ipv6_medium_instance_id    Specifies the IPv6 medium instance.
 *
 * @retval       NRF_SUCCESS    If the procedure was successful. Otherwise, a propagated 
 *                              error code is returned.
 *
 */
uint32_t ipv6_medium_connectable_mode_exit(ipv6_medium_instance_id_t ipv6_medium_instance_id);

/**@brief Function for getting the 48-bit Extended Unique Identifier.
 *
 * @param[in]    ipv6_medium_instance_id    Specifies the IPv6 medium instance.
 * @param[out]   p_ipv6_medium_eui48        Pointer to the EUI-48 value.
 *
 * @retval       NRF_SUCCESS    If the procedure was successful. Otherwise, a propagated 
 *                              error code is returned.
 *
 */
uint32_t ipv6_medium_eui48_get(ipv6_medium_instance_id_t   ipv6_medium_instance_id, \
                               eui48_t                   * p_ipv6_medium_eui48);

/**@brief Function for setting the 48-bit Extended Unique Identifier.
 *
 * @param[in]    ipv6_medium_instance_id    Specifies the IPv6 medium instance.
 * @param[in]    p_ipv6_medium_eui48        Pointer to the EUI-48 value.
 *
 * @retval       NRF_SUCCESS    If the procedure was successful. Otherwise, a propagated 
 *                              error code is returned.
 *
 */
uint32_t ipv6_medium_eui48_set(ipv6_medium_instance_id_t   ipv6_medium_instance_id, \
                               eui48_t                   * p_ipv6_medium_eui48);

/**@brief Function for getting the 64-bit Extended Unique Identifier.
 *
 * @param[in]    ipv6_medium_instance_id    Specifies the IPv6 medium instance.
 * @param[out]   p_ipv6_medium_eui64        Pointer to the EUI-64 value.
 *
 * @retval       NRF_SUCCESS    If the procedure was successful. Otherwise, a propagated 
 *                              error code is returned.
 *
 */
uint32_t ipv6_medium_eui64_get(ipv6_medium_instance_id_t   ipv6_medium_instance_id, \
                               eui64_t                   * p_ipv6_medium_eui64);

/**@brief Function for setting the 64-bit Extended Unique Identifier.
 *
 * @param[in]    ipv6_medium_instance_id    Specifies the IPv6 medium instance.
 * @param[in]    p_ipv6_medium_eui64        Pointer to the EUI-64 value.
 *
 * @retval       NRF_SUCCESS    If the procedure was successful. Otherwise, a propagated 
 *                              error code is returned.
 *
 */
uint32_t ipv6_medium_eui64_set(ipv6_medium_instance_id_t   ipv6_medium_instance_id, \
                               eui64_t                   * p_ipv6_medium_eui64);

#endif // IPV6_MEDIUM_H__

/** @} */
