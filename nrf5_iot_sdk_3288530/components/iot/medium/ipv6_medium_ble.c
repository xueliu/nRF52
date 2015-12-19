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

#include <stdint.h>
#include "ipv6_medium.h"
#include "ipv6_medium_ble.h"
#include "ble_advdata.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "app_trace.h"
#include "softdevice_handler_appsh.h"
#ifdef COMMISSIONING_ENABLED
#include "commissioning.h"
#endif // COMMISSIONING_ENABLED
#ifdef PSTORAGE_USED
#include "pstorage.h"
#endif // PSTORAGE_USED

#define PUBLIC_BLE_GAP_ADDR_CREATE_FROM_EUI64(ble_gap_addr, eui64, ble_gap_addr_type)              \
             ble_gap_addr[0] = eui64[7];                                                           \
             ble_gap_addr[1] = eui64[6];                                                           \
             ble_gap_addr[2] = eui64[5];                                                           \
             ble_gap_addr[3] = eui64[2];                                                           \
             ble_gap_addr[4] = eui64[1];                                                           \
             ble_gap_addr[5] = 0x00;                                                               \
             ble_gap_addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;

#define IOT_TIMER_DISABLE_API_PARAM_CHECK    0

#if (IOT_TIMER_DISABLE_API_PARAM_CHECK == 0)

#define NULL_PARAM_CHECK(PARAM)                                                                    \
        if ((PARAM) == NULL)                                                                       \
        {                                                                                          \
            return (NRF_ERROR_NULL);                                                               \
        }

#else // IOT_TIMER_DISABLE_API_PARAM_CHECK

#define NULL_PARAM_CHECK(PARAM)

#endif //IOT_TIMER_DISABLE_API_PARAM_CHECK

static ipv6_medium_instance_id_t         m_module_instance_id = 0x01;                               /**< Module instance identifier. As of today, only a single instance is supported. */
static ipv6_medium_evt_handler_t         m_ipv6_medium_evt_handler;                                 /**< Pointer to the event handler procedure of the parent layer. */
static ipv6_medium_error_handler_t       m_ipv6_medium_error_handler;                               /**< Pointer to the error handler procedure of the parent layer. */
static ble_gap_addr_t                    m_local_ble_addr;                                          /**< Local BT device address. */
static softdevice_evt_schedule_func_t    m_evt_schedule_func;                                       /**< Pointer to the procedure for scheduling events, if a scheduler is used. */
static ipv6_medium_ble_gap_params_t    * m_p_node_gap_params;                                       /**< Pointer to advertising parameters to be used. */
static ipv6_medium_ble_adv_params_t    * m_p_node_adv_params;                                       /**< Pointer to GAP parameters to be used. */

#ifndef COMMISSIONING_ENABLED
static ipv6_medium_ble_gap_params_t      m_gap_params;                                              /**< Advertising parameters w/o commissioning. */
static ipv6_medium_ble_adv_params_t      m_adv_params;                                              /**< GAP parameters w/o commissioning. */
static ble_uuid_t                        m_adv_uuids[] =
                                             {
                                                 {BLE_UUID_IPSP_SERVICE, BLE_UUID_TYPE_BLE}
                                             };                                                     /**< List of available service UUIDs in advertisement data. */
#else // COMMISSIONING_ENABLED
static uint16_t                          m_conn_handle = BLE_CONN_HANDLE_INVALID;                   /**< Handle of the active connection. */
static bool                              m_connectable_mode_active = false;                         /**< Indicates if the node is in connectable mode. */
static commissioning_id_mode_cb_t        m_commissioning_id_mode_cb;
static commissioning_poweroff_cb_t       m_commissioning_power_off_cb;
static bool                              m_adv_params_applied = false;                              /**< Indicates if advertising (and GAP) parameters have been applied. */
#endif // COMMISSIONING_ENABLED

#define BLE_MEDIUM_ENABLE_LOGS           1                                                          /**< Set to 1 to disable debug trace in the module. */

#if (BLE_MEDIUM_ENABLE_LOGS == 1)

#define MEDIUM_LOG  app_trace_log
#define MEDIUM_DUMP app_trace_dump

#else // BLE_MEDIUM_ENABLE_LOGS

#define MEDIUM_LOG(...)
#define MEDIUM_DUMP(...)

#endif // BLE_MEDIUM_ENABLE_LOGS

#define MEDIUM_ADDR(addr) MEDIUM_LOG("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\r\n",  \
                                     (addr).u8[0],(addr).u8[1],(addr).u8[2],(addr).u8[3],                            \
                                     (addr).u8[4],(addr).u8[5],(addr).u8[6],(addr).u8[7],                            \
                                     (addr).u8[8],(addr).u8[9],(addr).u8[10],(addr).u8[11],                          \
                                     (addr).u8[12],(addr).u8[13],(addr).u8[14],(addr).u8[15])


#ifndef COMMISSIONING_ENABLED

/**@brief Function for setting advertisement parameters.
 *
 * @details These parameters are applied if the Commissioning module is 
 *          not used or in Joining mode.
 */
static void adv_params_set(void)
{
    MEDIUM_LOG("[IPV6_MEDIUM]:  > adv_params_set\r\n");
    memset(&m_adv_params, 0x00, sizeof(m_adv_params));

    m_adv_params.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    m_adv_params.advdata.flags                   = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
    m_adv_params.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    m_adv_params.advdata.uuids_complete.p_uuids  = m_adv_uuids;

    m_adv_params.advparams.type        = BLE_GAP_ADV_TYPE_ADV_IND;
    m_adv_params.advparams.p_peer_addr = NULL;                             // Undirected advertisement.
    m_adv_params.advparams.fp          = BLE_GAP_ADV_FP_ANY;
    m_adv_params.advparams.interval    = APP_ADV_ADV_INTERVAL;
    m_adv_params.advparams.timeout     = APP_ADV_TIMEOUT;

    MEDIUM_LOG("[IPV6_MEDIUM]:  < adv_params_set\r\n");
}

/**@brief Function for setting GAP parameters.
 *
 * @details These parameters are applied if the Commissioning module is 
 *          not used or in Joining mode.
 */
static void gap_params_set(void)
{
    memset(&m_gap_params, 0x00, sizeof(m_gap_params));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&m_gap_params.sec_mode);

    m_gap_params.appearance = BLE_APPEARANCE_UNKNOWN;

    m_gap_params.p_dev_name   = (const uint8_t *)DEVICE_NAME;
    m_gap_params.dev_name_len = strlen(DEVICE_NAME);

    m_gap_params.gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    m_gap_params.gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    m_gap_params.gap_conn_params.slave_latency     = SLAVE_LATENCY;
    m_gap_params.gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
}

#endif // COMMISSIONING_ENABLED


/**@brief Function for applying the advertisement parameters.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 */
static void adv_params_apply(void)
{
    uint32_t err_code;

    err_code = ble_advdata_set(&m_p_node_adv_params->advdata, &m_p_node_adv_params->srdata);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for applying the GAP configuration.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_apply(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_device_name_set(&m_p_node_gap_params->sec_mode,     \
                                          m_p_node_gap_params->p_dev_name,    \
                                          m_p_node_gap_params->dev_name_len);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_appearance_set(m_p_node_gap_params->appearance);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_ppcp_set(&m_p_node_gap_params->gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the application's BLE Stack events and
 *        passing them on to the applications as generic transport medium events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    ipv6_medium_evt_t ipv6_medium_evt;

    memset(&ipv6_medium_evt, 0x00, sizeof(ipv6_medium_evt));
    ipv6_medium_evt.ipv6_medium_instance_id.ipv6_medium_instance_id   = m_module_instance_id;
    ipv6_medium_evt.ipv6_medium_instance_id.ipv6_medium_instance_type = IPV6_MEDIUM_ID_BLE;
    ipv6_medium_evt.medium_specific.ble.p_ble_evt                     = p_ble_evt;

    ipv6_medium_error_t ipv6_medium_error;
    memset(&ipv6_medium_error, 0x00, sizeof(ipv6_medium_error));
    ipv6_medium_error.ipv6_medium_instance_id.ipv6_medium_instance_id   = m_module_instance_id;
    ipv6_medium_error.ipv6_medium_instance_id.ipv6_medium_instance_type = IPV6_MEDIUM_ID_BLE;

    bool do_notify_event = false;
    bool do_notify_error = false;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
        {
#ifdef COMMISSIONING_ENABLED
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
#endif // COMMISSIONING_ENABLED
            ipv6_medium_evt.ipv6_medium_evt_id = IPV6_MEDIUM_EVT_CONN_UP;
            do_notify_event = true;

            break;
        }
        case BLE_GAP_EVT_DISCONNECTED:
        {
#ifdef COMMISSIONING_ENABLED
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
#endif // COMMISSIONING_ENABLED
            ipv6_medium_evt.ipv6_medium_evt_id = IPV6_MEDIUM_EVT_CONN_DOWN;
            do_notify_event = true;

            break;
        }
        case BLE_GAP_EVT_TIMEOUT:
        {
            if (p_ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISING)
            {
                ipv6_medium_evt.ipv6_medium_evt_id = IPV6_MEDIUM_EVT_CONNECTABLE_MODE_EXIT;
                do_notify_event = true;
            }
            else
            {
                // This is not necessarily an error, only added here to show error handler usage.
                ipv6_medium_error.medium_specific.ble.dummy_value = 0x13;
                do_notify_error = true;
            }
            break;
        }
        default:
        {
            ipv6_medium_evt.ipv6_medium_evt_id = IPV6_MEDIUM_EVT_PHY_SPECIFIC;
            do_notify_event = true;

            break;
        }
    }

    ble_ipsp_evt_handler(p_ble_evt);
    
    if (do_notify_event == true)
    {
        m_ipv6_medium_evt_handler(&ipv6_medium_evt);
    }
    if (do_notify_error == true)
    {
        m_ipv6_medium_error_handler(&ipv6_medium_error);
    }    
}


/**@brief     Function for dispatching a BLE SoftDevice event to all modules with a BLE 
 *            SoftDevice event handler.
 *
 * @details   This function is called from the SoftDevice event interrupt handler after a
 *            SoftDevice event has been received.
 *
 * @param[in] p_ble_evt BLE SoftDevice event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
#ifdef COMMISSIONING_ENABLED
    commissioning_ble_evt_handler(p_ble_evt);
    ble_ncfgs_ble_evt_handler(p_ble_evt);
#endif // COMMISSIONING_ENABLED

    on_ble_evt(p_ble_evt);
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void sys_evt_dispatch(uint32_t event)
{
#if (defined COMMISSIONING_ENABLED) || (defined PSTORAGE_USED)
    pstorage_sys_event_handler(event);
#endif // COMMISSIONING_ENABLED || PSTORAGE_USED
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static uint32_t ble_stack_init(void)
{
    uint32_t err_code = NRF_SUCCESS;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, m_evt_schedule_func);

    // Enable BLE stack.
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0x00, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT;
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = sd_ble_enable(&ble_enable_params);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Register with the SoftDevice handler module for System events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return err_code;
}


uint32_t ipv6_medium_connectable_mode_enter(ipv6_medium_instance_id_t ipv6_medium_instance_id)
{
    MEDIUM_LOG("[IPV6_MEDIUM]:  > ipv6_medium_connectable_mode_enter\r\n");

    if (ipv6_medium_instance_id != m_module_instance_id)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

#ifdef COMMISSIONING_ENABLED
    if (m_adv_params_applied == false)
    {
        // Apply advertising (and GAP) parameters, if not applied when node mode changed.
        commissioning_gap_params_get(&m_p_node_gap_params);
        commissioning_adv_params_get(&m_p_node_adv_params);
        gap_params_apply();
        adv_params_apply();
    }
    m_adv_params_applied = false;
#endif // COMMISSIONING_ENABLED

    uint32_t err_code = sd_ble_gap_adv_start(&m_p_node_adv_params->advparams);
#ifdef COMMISSIONING_ENABLED
    if (err_code == NRF_SUCCESS)
    {
        m_connectable_mode_active = true;
    }
#endif // COMMISSIONING_ENABLED
    MEDIUM_LOG("[IPV6_MEDIUM]:  < ipv6_medium_connectable_mode_enter\r\n");
    return err_code;
}


uint32_t ipv6_medium_connectable_mode_exit(ipv6_medium_instance_id_t ipv6_medium_instance_id)
{
    if (ipv6_medium_instance_id != m_module_instance_id)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    uint32_t err_code = sd_ble_gap_adv_stop();
#ifdef COMMISSIONING_ENABLED
    if (err_code == NRF_SUCCESS)
    {
        m_connectable_mode_active = false;
    }
#endif // COMMISSIONING_ENABLED
    return err_code;
}


uint32_t ipv6_medium_eui48_get(ipv6_medium_instance_id_t   ipv6_medium_instance_id, \
                               eui48_t                   * p_ipv6_medium_eui48)
{
    if (ipv6_medium_instance_id != m_module_instance_id)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    ble_gap_addr_t local_ble_addr;
    uint32_t err_code = sd_ble_gap_address_get(&local_ble_addr);
    
    memcpy(p_ipv6_medium_eui48->identifier, local_ble_addr.addr, EUI_48_SIZE);

    return err_code;
}


uint32_t ipv6_medium_eui48_set(ipv6_medium_instance_id_t   ipv6_medium_instance_id, \
                               eui48_t                   * p_ipv6_medium_eui48)
{
    if (ipv6_medium_instance_id != m_module_instance_id)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (p_ipv6_medium_eui48->identifier[5] != 0x00)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    m_local_ble_addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;
    memcpy(m_local_ble_addr.addr, p_ipv6_medium_eui48->identifier, EUI_48_SIZE);

    return sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &m_local_ble_addr);
}


uint32_t ipv6_medium_eui64_get(ipv6_medium_instance_id_t   ipv6_medium_instance_id, \
                               eui64_t                   * p_ipv6_medium_eui64)
{
    if (ipv6_medium_instance_id != m_module_instance_id)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    ble_gap_addr_t local_ble_addr;

    uint32_t err_code = sd_ble_gap_address_get(&local_ble_addr);
    APP_ERROR_CHECK(err_code);

    IPV6_EUI64_CREATE_FROM_EUI48(p_ipv6_medium_eui64->identifier,
                                 local_ble_addr.addr,
                                 local_ble_addr.addr_type);
    return NRF_SUCCESS;
}


uint32_t ipv6_medium_eui64_set(ipv6_medium_instance_id_t   ipv6_medium_instance_id, \
                               eui64_t                   * p_ipv6_medium_eui64)
{
    if (ipv6_medium_instance_id != m_module_instance_id)
    {
        return NRF_ERROR_INVALID_PARAM;
    }
    if ((p_ipv6_medium_eui64->identifier[0] != 0x02)  ||
        (p_ipv6_medium_eui64->identifier[3] != 0xFF)  ||
        (p_ipv6_medium_eui64->identifier[4] != 0xFE))
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    ble_gap_addr_t local_ble_addr;

    PUBLIC_BLE_GAP_ADDR_CREATE_FROM_EUI64(local_ble_addr.addr, \
                                          p_ipv6_medium_eui64->identifier, \
                                          local_ble_addr.addr_type);

    return sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &local_ble_addr);
}


#ifdef COMMISSIONING_ENABLED

void commissioning_evt_handler(commissioning_evt_t * p_commissioning_evt)
{
    MEDIUM_LOG("[IPV6_MEDIUM]:  > commissioning_evt_handler\r\n");

    switch (p_commissioning_evt->commissioning_evt_id)
    {
        case COMMISSIONING_EVT_CONFIG_MODE_ENTER:
            // Fall-through.
        case COMMISSIONING_EVT_JOINING_MODE_ENTER:
        {
            m_commissioning_power_off_cb(p_commissioning_evt->power_off_enable_requested);

            if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
            {
                // Making sure that advertising (and GAP) parameters are 
                // applied when entering connectable mode the next time. 
                m_adv_params_applied = false;
                UNUSED_VARIABLE(sd_ble_gap_disconnect(m_conn_handle, \
                                                      BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION));
            }
            else
            {
                bool do_return_to_connectable_mode = m_connectable_mode_active;
                UNUSED_VARIABLE(ipv6_medium_connectable_mode_exit(m_module_instance_id));

                commissioning_gap_params_get(&m_p_node_gap_params);
                commissioning_adv_params_get(&m_p_node_adv_params);
                gap_params_apply();
                adv_params_apply();
                // Advertising and GAP parameters applied, making sure that 
                // it is not repeated when entering connectable mode the next time. 
                m_adv_params_applied = true;

                if (do_return_to_connectable_mode == true)
                {
                    // Restart connectable mode, if the node was in connectable mode appliying the new params. 
                    UNUSED_VARIABLE(ipv6_medium_connectable_mode_enter(m_module_instance_id));
                }
            }
            
            break;
        }
        case COMMISSIONING_EVT_IDENTITY_MODE_ENTER:
        {
            m_commissioning_id_mode_cb(CMD_IDENTITY_MODE_ENTER);

            break;
        }
        case COMMISSIONING_EVT_IDENTITY_MODE_EXIT:
        {
            m_commissioning_id_mode_cb(CMD_IDENTITY_MODE_EXIT);

            break;
        }
        default:
        {
            // No implementation needed.
            break;
        }
    }

    MEDIUM_LOG("[IPV6_MEDIUM]:  < commissioning_evt_handler\r\n");
}

#endif // COMMISSIONING_ENABLED


uint32_t ipv6_medium_init(ipv6_medium_init_params_t * p_init_param,          \
                          ipv6_medium_type_t          desired_medium_type,   \
                          ipv6_medium_instance_t    * p_new_medium_instance)
{
    MEDIUM_LOG("[IPV6_MEDIUM]:  > init\r\n");
    uint32_t err_code = NRF_SUCCESS;
    NULL_PARAM_CHECK(p_init_param->ipv6_medium_evt_handler);
    if (desired_medium_type != IPV6_MEDIUM_ID_BLE)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    m_ipv6_medium_evt_handler   = p_init_param->ipv6_medium_evt_handler;
    m_ipv6_medium_error_handler = p_init_param->ipv6_medium_error_handler;
    if (p_init_param->use_scheduler == false)
    {
        m_evt_schedule_func = NULL;
    }
    else 
    {
        m_evt_schedule_func = NULL;
    }

    p_new_medium_instance->ipv6_medium_instance_type = IPV6_MEDIUM_ID_BLE;
    p_new_medium_instance->ipv6_medium_instance_id   = m_module_instance_id;

    err_code = ble_stack_init();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

#ifndef COMMISSIONING_ENABLED
    gap_params_set();
    adv_params_set();
    m_p_node_gap_params = &m_gap_params;
    m_p_node_adv_params = &m_adv_params;
    gap_params_apply();
    adv_params_apply();
#else // COMMISSIONING_ENABLED
    m_commissioning_id_mode_cb   = p_init_param->commissioning_id_mode_cb;
    m_commissioning_power_off_cb = p_init_param->commissioning_power_off_cb;

    commissioning_init_params_t init_param;
    memset(&init_param, 0x00, sizeof(init_param));
    init_param.commissioning_evt_handler = commissioning_evt_handler;
    uint8_t new_mode;
    err_code = commissioning_init(&init_param, \
                                  &new_mode);

    commissioning_node_mode_change(new_mode);
#endif // COMMISSIONING_ENABLED

    MEDIUM_LOG("[IPV6_MEDIUM]:  < init\r\n");
    return err_code;
}
