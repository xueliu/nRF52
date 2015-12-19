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

#ifdef COMMISSIONING_ENABLED

#include <string.h>
#include "app_trace.h"
#include "ble_ncfgs.h"
#include "app_error.h"
#include "ble.h"
#include "nordic_common.h"

/**@breif NCFGS database encapsulation.  */
typedef struct
{
    uint16_t                     service_handle;
    ble_gatts_char_handles_t     ssid_handles;
    ble_gatts_char_handles_t     keys_store_handles;
    ble_gatts_char_handles_t     ctrlp_handles;
} ble_database_t;

static ble_ncfgs_state_t         m_service_state = NCFGS_STATE_IDLE;                                /**< Module state value. */
static ble_ncfgs_evt_handler_t   m_app_evt_handler;                                                 /**< Parent module callback function. */
static ble_database_t            m_database;                                                        /**< GATT handles database. */
static uint8_t                   m_ctrlp_value_buffer[NCFGS_CTRLP_VALUE_LEN];                       /**< Stores received Control Point value before parsing. */
static ble_ncfgs_data_t          m_ncfgs_data;                                                      /**< Stores all values written by the peer device. */

#define NCFGS_ENABLE_LOGS        1                                                                  /**< Set to 1 to disable debug trace in the module. */

#if (NCFGS_ENABLE_LOGS == 1)

#define NCFGS_LOG  app_trace_log
#define NCFGS_DUMP app_trace_dump

#else // NCFGS_ENABLE_LOGS

#define NCFGS_LOG(...)
#define NCFGS_DUMP(...)

#endif // NCFGS_ENABLE_LOGS

/**@brief Function for adding the SSID Store characteristic.
 *
 * @return    NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t add_ssid_characteristic(ble_uuid_t * p_srv_uuid)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          char_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0x00, sizeof(char_md));

    char_md.char_props.read  = 1;
    char_md.char_props.write = 1;

    memset(&attr_md, 0x00, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.wr_auth = 1;
    attr_md.vloc    = BLE_GATTS_VLOC_USER;

    memset(&attr_char_value, 0x00, sizeof(attr_char_value));

    char_uuid.type = p_srv_uuid->type;
    char_uuid.uuid = BLE_UUID_NCFGS_SSID_CHAR;

    attr_char_value.p_uuid    = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = NCFGS_SSID_MAX_LEN;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = NCFGS_SSID_MAX_LEN;
    attr_char_value.p_value   = &m_ncfgs_data.ssid_from_router.ssid[0];

    return sd_ble_gatts_characteristic_add(m_database.service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &m_database.ssid_handles);
}


/**@brief Function for adding the Keys Store characteristic.
 *
 * @return    NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t add_keys_store_characteristic(ble_uuid_t * p_srv_uuid)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          char_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0x00, sizeof(char_md));

    char_md.char_props.read  = 1;
    char_md.char_props.write = 1;

    memset(&attr_md, 0x00, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.wr_auth    = 1;
    attr_md.vloc       = BLE_GATTS_VLOC_USER;

    memset(&attr_char_value, 0x00, sizeof(attr_char_value));

    char_uuid.type = p_srv_uuid->type;
    char_uuid.uuid = BLE_UUID_NCFGS_KEYS_STORE_CHAR;

    attr_char_value.p_uuid    = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = NCFGS_KEYS_MAX_LEN;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = NCFGS_KEYS_MAX_LEN;
    attr_char_value.p_value   = &m_ncfgs_data.keys_from_router.keys[0];

    return sd_ble_gatts_characteristic_add(m_database.service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &m_database.keys_store_handles);
}


/**@brief Function for adding the Control Point characteristic.
 *
 * @return    NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t add_ip_cfg_cp_characteristic(ble_uuid_t * p_srv_uuid)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          char_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0x00, sizeof(char_md));

    char_md.char_props.read  = 1;
    char_md.char_props.write = 1;

    memset(&attr_md, 0x00, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.wr_auth    = 1;
    attr_md.vloc       = BLE_GATTS_VLOC_USER;

    memset(&attr_char_value, 0x00, sizeof(attr_char_value));

    char_uuid.type = p_srv_uuid->type;
    char_uuid.uuid = BLE_UUID_NCFGS_CTRLPT_CHAR;

    attr_char_value.p_uuid    = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = NCFGS_CTRLP_VALUE_LEN;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = NCFGS_CTRLP_VALUE_LEN;
    attr_char_value.p_value   = &m_ctrlp_value_buffer[0];

    return sd_ble_gatts_characteristic_add(m_database.service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &m_database.ctrlp_handles);
}


/**@brief Function for creating the GATT database.
 *
 * @return    NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t ble_ncfgs_create_database(void)
{
    uint32_t   err_code = NRF_SUCCESS;

    // Add service.
    ble_uuid_t service_uuid;

    const ble_uuid128_t base_uuid128 =
    {
        {
            0x73, 0x3E, 0x2D, 0x02, 0xB7, 0x6B, 0xBE, 0xBE, \
            0xE5, 0x4F, 0x40, 0x8F, 0x00, 0x00, 0x20, 0x54
        }
    };

    service_uuid.uuid = BLE_UUID_NODE_CFG_SERVICE;

    err_code = sd_ble_uuid_vs_add(&base_uuid128, &(service_uuid.type));
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,  \
                                        &service_uuid,                \
                                        &m_database.service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    err_code = add_ssid_characteristic(&service_uuid);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    err_code = add_keys_store_characteristic(&service_uuid);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    err_code = add_ip_cfg_cp_characteristic(&service_uuid);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    return err_code;
}


uint32_t ble_ncfgs_init(ble_ncfgs_evt_handler_t ble_ncfgs_cb)
{
    NCFGS_LOG("[NCFGS]: > Init \r\n");
    uint32_t err_code;
    memset(&m_ncfgs_data, 0x00, sizeof(m_ncfgs_data));

    m_app_evt_handler = ble_ncfgs_cb;

    err_code = ble_ncfgs_create_database();
    NCFGS_LOG("[NCFGS]: < Init \r\n");
    return err_code;
}


/**@brief Function for decoding the Control Point characteristic value.
 *
 * @return    NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t ctrlp_value_decode(const ble_evt_t * p_ble_evt)
{
    uint8_t wr_req_value_len = \
                p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.len;

    memcpy(m_ctrlp_value_buffer,                                                  \
           p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.data,  \
           wr_req_value_len);

    m_ncfgs_data.ctrlp_value.opcode           = \
        (ble_ncfgs_opcode_t)m_ctrlp_value_buffer[0];
    memcpy((void *)&m_ncfgs_data.ctrlp_value.delay_sec,  \
           &m_ctrlp_value_buffer[NCFGS_CTRLP_OPCODE_LEN],      \
           sizeof(uint32_t));
    m_ncfgs_data.ctrlp_value.delay_sec        = \
        HTONL(m_ncfgs_data.ctrlp_value.delay_sec);
    memcpy((void *)&m_ncfgs_data.ctrlp_value.duration_sec,           \
           &m_ctrlp_value_buffer[NCFGS_CTRLP_OPCODE_LEN+NCFGS_CTRLP_DELAY_LEN],  \
           sizeof(uint32_t));
    m_ncfgs_data.ctrlp_value.duration_sec     = \
        HTONL(m_ncfgs_data.ctrlp_value.duration_sec);
    m_ncfgs_data.ctrlp_value.state_on_failure = \
        (state_on_failure_t)m_ctrlp_value_buffer[NCFGS_CTRLP_OPCODE_LEN+    \
                                                 NCFGS_CTRLP_DELAY_LEN+     \
                                                 NCFGS_CTRLP_DURATION_LEN];

    if ((m_ncfgs_data.ctrlp_value.state_on_failure != NCFGS_SOF_NO_CHANGE)    && \
        (m_ncfgs_data.ctrlp_value.state_on_failure != NCFGS_SOF_PWR_OFF)      && \
        (m_ncfgs_data.ctrlp_value.state_on_failure != NCFGS_SOF_CONFIG_MODE))
    {
        return NRF_ERROR_INVALID_DATA;
    }

    uint8_t id_data_len = wr_req_value_len - NCFGS_CTRLP_ALL_BUT_ID_DATA_LEN;
    if (id_data_len != 0)
    {
        m_ncfgs_data.id_data.identity_data_len = id_data_len;

        memcpy(m_ncfgs_data.id_data.identity_data,                      \
               &m_ctrlp_value_buffer[NCFGS_CTRLP_ALL_BUT_ID_DATA_LEN],  \
               id_data_len);
    }

    return NRF_SUCCESS;
}


void ble_ncfgs_ble_evt_handler(const ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            if (p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op == \
                    BLE_GATTS_OP_WRITE_REQ)
            {
                ble_gatts_rw_authorize_reply_params_t reply_params;
                memset(&reply_params, 0x00, sizeof(reply_params));
                reply_params.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                uint16_t wr_req_handle = \
                    p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.handle;
                uint8_t wr_req_value_len = \
                    p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.len;

                if (wr_req_handle == m_database.ssid_handles.value_handle)
                {
                    NCFGS_LOG("[NCFGS]: > wr_req: ssid_handle \r\n");

                    if ((wr_req_value_len > NCFGS_SSID_MAX_LEN)  || \
                        (wr_req_value_len < NCFGS_SSID_MIN_LEN))
                    {
                        reply_params.params.write.gatt_status = \
                            BLE_GATT_STATUS_ATTERR_INVALID_ATT_VAL_LENGTH;
                    }
                    else
                    {
                        m_ncfgs_data.ssid_from_router.ssid_len = wr_req_value_len;
                        m_service_state |= NCFGS_STATE_SSID_WRITTEN;

                        reply_params.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
                    }

                    UNUSED_VARIABLE( \
                        sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gap_evt.conn_handle, \
                                                        &reply_params));
                    NCFGS_LOG("[NCFGS]: < wr_req: ssid_handle \r\n");
                    return;
                }

                else if (wr_req_handle == m_database.keys_store_handles.value_handle)
                {
                    NCFGS_LOG("[NCFGS]: > wr_req: keys_store_handle \r\n");

                    if (wr_req_value_len > NCFGS_KEYS_MAX_LEN)
                    {
                        reply_params.params.write.gatt_status  = \
                            BLE_GATT_STATUS_ATTERR_INVALID_ATT_VAL_LENGTH;
                    }
                    else
                    {
                        m_ncfgs_data.keys_from_router.keys_len = wr_req_value_len;
                        m_service_state |= NCFGS_STATE_KEYS_STORE_WRITTEN;
                        reply_params.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
                    }

                    UNUSED_VARIABLE( \
                        sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gap_evt.conn_handle, \
                                                        &reply_params));
                    NCFGS_LOG("[NCFGS]: < wr_req: keys_store_handle \r\n");
                    return;
                }

                else if (wr_req_handle == m_database.ctrlp_handles.value_handle)
                {
                    NCFGS_LOG("[NCFGS]: > wr_req: ctrlp_handle \r\n");

                    bool notify_app = false;

                    if ((wr_req_value_len > NCFGS_CTRLP_VALUE_LEN)            || \
                        (wr_req_value_len < NCFGS_CTRLP_ALL_BUT_ID_DATA_LEN))
                    {
                        reply_params.params.write.gatt_status = \
                            BLE_GATT_STATUS_ATTERR_INVALID_ATT_VAL_LENGTH;
                    }
                    else
                    {
                        ble_ncfgs_opcode_t opcode_in = (ble_ncfgs_opcode_t) \
                            p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.data[0];

                        reply_params.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;

                        if ((opcode_in != NCFGS_OPCODE_GOTO_JOINING_MODE)      &&  \
                            (opcode_in != NCFGS_OPCODE_GOTO_CONFIG_MODE)       &&  \
                            (opcode_in != NCFGS_OPCODE_GOTO_IDENTITY_MODE))
                        {
                            reply_params.params.write.gatt_status = APP_GATTERR_UNKNOWN_OPCODE;
                        }

                        if (opcode_in == NCFGS_OPCODE_GOTO_JOINING_MODE)
                        {
                            if (!((m_service_state & NCFGS_STATE_SSID_WRITTEN)         && \
                                  (m_service_state & NCFGS_STATE_KEYS_STORE_WRITTEN)))
                            {
                                reply_params.params.write.gatt_status = APP_GATTERR_NOT_CONFIGURED;
                            }
                        }

                        if (reply_params.params.write.gatt_status == BLE_GATT_STATUS_SUCCESS)
                        {
                            err_code = ctrlp_value_decode(p_ble_evt);

                            if (err_code != NRF_SUCCESS)
                            {
                                reply_params.params.write.gatt_status = \
                                    APP_GATTERR_INVALID_ATTR_VALUE;
                            }
                            else
                            {
                                notify_app = true;
                            }
                        }
                    }

                    UNUSED_VARIABLE(sd_ble_gatts_rw_authorize_reply(        \
                                        p_ble_evt->evt.gap_evt.conn_handle, \
                                        &reply_params));

                    if (notify_app == true)
                    {
                        NCFGS_LOG("[NCFGS]: > do notify parent \r\n");

                        m_app_evt_handler(&m_ncfgs_data);

                        NCFGS_LOG("[NCFGS]: < do notify parent \r\n");
                    }
                    NCFGS_LOG("[NCFGS]: < wr_req: ctrlp_handle \r\n");
                }
                else
                {
                    // Invalid handle.
                    reply_params.params.write.gatt_status = BLE_GATT_STATUS_ATTERR_INVALID_HANDLE;
                    UNUSED_VARIABLE(sd_ble_gatts_rw_authorize_reply( \
                                        p_ble_evt->evt.gap_evt.conn_handle, &reply_params));
                }
            }

            break;
        }
        default:
        {
            break;
        }
    }
}

#endif // COMMISSIONING_ENABLED
