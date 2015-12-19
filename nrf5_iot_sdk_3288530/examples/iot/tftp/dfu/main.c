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

/** @file
 *
 * @defgroup iot_sdk_tftp_dfu_app main.c
 * @{
 * @ingroup iot_sdk_app_ipv6
 * @brief This file contains the source code for Nordic's IPv6 DFU over TFTP sample application.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "nordic_common.h"
#include "nrf_delay.h"
#include "softdevice_handler.h"
#include "mem_manager.h"
#include "app_trace.h"
#include "app_timer.h"
#include "app_button.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "pstorage.h"
#include "iot_common.h"
#include "iot_timer.h"
#include "iot_dfu.h"
#include "iot_tftp.h"
#include "iot_file.h"
#include "iot_file_static.h"
#include "ipv6_api.h"
#include "icmp6_api.h"
#include "cJSON.h"
#include "cJSON_iot_hooks.h"
#include "crc16.h"
#include "ipv6_medium.h"

#define LED_ONE                         BSP_LED_0_MASK
#define LED_TWO                         BSP_LED_1_MASK
#define LED_THREE                       BSP_LED_2_MASK
#define LED_FOUR                        BSP_LED_3_MASK
#define ALL_APP_LED                    (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                        BSP_LED_2_MASK | BSP_LED_3_MASK)                            /**< Define used for simultaneous operation of all application LEDs. */

#define START_BUTTON_PIN_NO             BSP_BUTTON_0                                                /**< Button used to start application state machine. */
#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)                    /**< Button detection delay. */

#define APP_TIMER_OP_QUEUE_SIZE         5                                                           /**< Size of timer operation queues. */

#define LED_BLINK_INTERVAL_MS           500                                                         /**< LED blinking interval. */
#define APP_DFU_POLLING_INTERVAL        APP_TIMER_TICKS(10000, APP_TIMER_PRESCALER)                 /**< Interval of periodic download of the configuration file. */

#define APP_RTR_SOLICITATION_DELAY      1000                                                        /**< Time before host sends an initial solicitation in ms. */

#define DEAD_BEEF                       0xDEADBEEF                                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define APP_ENABLE_LOGS                 1                                                           /**< Disable debug trace in the application. */
#define MAX_LENGTH_FILENAME             32                                                          /**< Maximum length of the filename. */
#define MAX_CONFIG_SIZE                 256                                                         /**< Maximum DFU of the config size. */

/**@brief JSON main object field names. */
#define CONFIG_APP_KEY                  "app"
#define CONFIG_SD_KEY                   "sd"
#define CONFIG_APPSD_KEY                "appsd"
#define CONFIG_BL_KEY                   "bl"
#define CONFIG_PATH_KEY                 "p"
#define CONFIG_SIZE_KEY                 "s"
#define CONFIG_ID_KEY                   "id"

#define APP_TFTP_LOCAL_PORT             100                                                         /**< Local UDP port for TFTP client usage. */
#define APP_TFTP_SERVER_PORT            69                                                          /**< UDP port on which TFTP server listens. */
#define APP_TFTP_BLOCK_SIZE             512                                                         /**< Maximum or negotiated size of data block. */
#define APP_TFTP_RETRANSMISSION_TIME    3                                                           /**< Number of milliseconds between retransmissions. */

#if (APP_ENABLE_LOGS == 1)

#define APPL_LOG  app_trace_log
#define APPL_DUMP app_trace_dump

#else // APP_ENABLE_LOGS

#define APPL_LOG(...)
#define APPL_DUMP(...)

#endif // APP_ENABLE_LOGS

#define APPL_ADDR(addr) APPL_LOG("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\r\n", \
                                (addr).u8[0],(addr).u8[1],(addr).u8[2],(addr).u8[3],                            \
                                (addr).u8[4],(addr).u8[5],(addr).u8[6],(addr).u8[7],                            \
                                (addr).u8[8],(addr).u8[9],(addr).u8[10],(addr).u8[11],                          \
                                (addr).u8[12],(addr).u8[13],(addr).u8[14],(addr).u8[15])

/**@brief Led's indication state. */
typedef enum
{
    LEDS_INACTIVE = 0,
    LEDS_CONNECTABLE_MODE,
    LEDS_IPV6_IF_DOWN,
    LEDS_IPV6_IF_UP
} display_state_t;

/**@brief Application's DFU state. */
typedef enum
{
    APP_DFU_STATE_IDLE = 0,                                                                            /**@brief DFU state indicating that application does not perform any DFU task. */    
    APP_DFU_STATE_GET_CONFIG,                                                                          /**@brief DFU state indicating that device is in downloading of config procedure. */
    APP_DFU_STATE_GET_FIRMWARE,                                                                        /**@brief DFU state indicating that device is in downloading of firmwareprocedure. */
} app_dfu_state_t;

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                                /**< App timer instance used to update the IoT timer wall clock. */
APP_TIMER_DEF(m_iot_dfu_timer_id);                                                                     /**< App timer instance used to make a periodic poll to check if the new software is available. */

static ipv6_medium_instance_t    m_ipv6_medium;
eui64_t                          eui64_local_iid;                                                      /**< Local EUI64 value that is used as the IID for*/
static iot_interface_t         * mp_interface = NULL;                                                  /**< Pointer to IoT interface if any. */
static display_state_t           m_display_state = LEDS_INACTIVE;                                      /**< Board LED display state. */
static iot_tftp_t                m_tftp;                                                               /**< TFTP instance used for DFU. */
static app_dfu_state_t           m_dfu_state = APP_DFU_STATE_IDLE;                                     /**< Actual state of DFU application. */
static cJSON                   * mp_config_json;                                                       /**< Pointer of cJSON instance. */
static iot_file_t                m_config_file;                                                        /**< Pointer to the file used for config of DFU. */
static iot_file_t              * mp_firmware_file;                                                     /**< Pointer to the file used for image of DFU. */
static iot_dfu_firmware_desc_t   m_firmware_desc;

static uint8_t                   m_config_mem[MAX_CONFIG_SIZE];
static char                      m_config_filename[MAX_LENGTH_FILENAME];
static char                      m_firmware_filename[MAX_LENGTH_FILENAME];


/**@brief Addresses used in sample application. */
static const ipv6_addr_t         m_local_routers_multicast_addr = {{0xFF, 0x02, 0x00, 0x00, \
                                                                    0x00, 0x00, 0x00, 0x00, \
                                                                    0x00, 0x00, 0x00, 0x00, \
                                                                    0x00, 0x00, 0x00, 0x02}};          /**< Multicast address of all routers on the local network segment. */


#ifdef COMMISSIONING_ENABLED
static bool                      m_power_off_on_failure = false;
static bool                      m_identity_mode_active;
#endif // COMMISSIONING_ENABLED

/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 */
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    LEDS_ON(ALL_APP_LED);
    APPL_LOG("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s\r\n", \
             error_code, line_num, p_file_name);

    // Variables used to retain function parameter values when debugging.
    static volatile uint8_t  s_file_name[MAX_LENGTH_FILENAME];
    static volatile uint16_t s_line_num;
    static volatile uint32_t s_error_code;

    strncpy((char *)s_file_name, (const char *)p_file_name, MAX_LENGTH_FILENAME - 1);
    s_file_name[MAX_LENGTH_FILENAME - 1] = '\0';
    s_line_num                           = line_num;
    s_error_code                         = error_code;
    UNUSED_VARIABLE(s_file_name);
    UNUSED_VARIABLE(s_line_num);
    UNUSED_VARIABLE(s_error_code);

#ifdef COMMISSIONING_ENABLED
    if (m_power_off_on_failure == true)
    {
        LEDS_OFF((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
        UNUSED_VARIABLE(sd_power_system_off());
    }
    else
    {
        NVIC_SystemReset();
    }
#else // COMMISSIONING_ENABLED
    //NVIC_SystemReset();

    for (;;)
    {
    }
#endif // COMMISSIONING_ENABLED
}


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by this application.
 */
static void leds_init(void)
{
    // Configure leds.
    LEDS_CONFIGURE((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));

    // Turn leds off.
    LEDS_OFF((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
}


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void blink_timeout_handler(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);

#ifdef COMMISSIONING_ENABLED
    static bool id_mode_previously_enabled;
#endif // COMMISSIONING_ENABLED

    switch (m_display_state)
    {
        case LEDS_INACTIVE:
        {
            LEDS_OFF((LED_ONE | LED_TWO));
            break;
        }

        case LEDS_CONNECTABLE_MODE:
        {
            LEDS_INVERT(LED_ONE);
            LEDS_OFF(LED_TWO);
            break;
        }

        case LEDS_IPV6_IF_DOWN:
        {
            LEDS_OFF((LED_THREE | LED_FOUR));
            LEDS_ON(LED_ONE);
            LEDS_INVERT(LED_TWO);
            break;
        }

        case LEDS_IPV6_IF_UP:
        {
            LEDS_OFF(LED_ONE);
            LEDS_ON(LED_TWO);
            break;
        }

        default:
            break;
    }

#ifdef COMMISSIONING_ENABLED
    if ((id_mode_previously_enabled == false) && (m_identity_mode_active == true))
    {
        LEDS_OFF(LED_THREE | LED_FOUR);
    }

    if ((id_mode_previously_enabled == true) && (m_identity_mode_active == true))
    {
        LEDS_INVERT(LED_THREE | LED_FOUR);
    }

    if ((id_mode_previously_enabled == true) && (m_identity_mode_active == false))
    {
        LEDS_OFF(LED_THREE | LED_FOUR);
    }

    id_mode_previously_enabled = m_identity_mode_active;
#endif // COMMISSIONING_ENABLED
}


/**@brief Function for checking current firmware in terms of new, downloaded application.
 *
 * @param[in]  p_image Pointer to memory, where current firmware is stored.
 * @param[in]  key     Key inside JSON configuration file, describing firmware.
 * @param[out] p_crc   CRC16 of new firmware.
 * 
 * @return None.
 */
static void check_bin(uint8_t * p_image, const char * p_key, uint32_t * p_size, uint32_t * p_crc)
{
    uint32_t   size;
    uint16_t   crc;
    uint16_t   current_crc;
    cJSON    * p_cursor;
    cJSON    * p_cursor_back;

    // Clear output parameters.
    *p_size = 0;
    *p_crc  = 0;

    if (mp_config_json == NULL)
    {
        APPL_LOG("[APPL]: Invalid JSON file!\r\n");
        return;
    }

    p_cursor = cJSON_GetObjectItem(mp_config_json, (const char *)p_key);
    if (p_cursor != NULL)
    {
        p_cursor_back = p_cursor;
        p_cursor      = cJSON_GetObjectItem(p_cursor, CONFIG_SIZE_KEY);

        if (p_cursor != NULL)
        {
            if ((p_cursor->type != cJSON_Number) || (p_cursor->valueint == 0))
            {
                return;
            }

            size = p_cursor->valueint;
        }
        else
        {
            APPL_LOG("[APPL]: No binary SIZE inside JSON.\r\n");
            return;
        }

        p_cursor = p_cursor_back;
        p_cursor = cJSON_GetObjectItem(p_cursor, CONFIG_ID_KEY);
        if (p_cursor != NULL)
        {
            if (p_cursor->type != cJSON_Number)
            {
                return;
            }
            
            crc    = (uint16_t)p_cursor->valueint;
            *p_crc = crc;
        }
        else
        {
            APPL_LOG("[APPL]: No binary ID inside JSON.\r\n");
            return;
        }

        current_crc = crc16_compute(p_image, size, NULL);
        if (current_crc != crc)
        {
            APPL_LOG("[APPL]: CRC mismatch: current: %d <> %d.\r\n", current_crc, crc);
            *p_size = size;
        }
    }
    else
    {
        APPL_LOG("[APPL]: No binary KEY inside JSON.\r\n");
    }
}


/**@brief Function for reading binary path from JSON configuration.
 *
 * @param[in]  p_dst_str Pointer to memory, where path should be stored.
 * @param[in]  key       Key inside JSON configuration file, describing firmware.
 * 
 * @return    NRF_SUCCESS if path found and stored in p_dst_str, otherwise error code.
 */
static uint32_t get_path(char * p_dst_str, const char * p_key)
{
    cJSON * p_cursor;

    if ((mp_config_json == NULL) || (p_dst_str == NULL))
    {
        APPL_LOG("[APPL]: Invalid parameters!\r\n");
        return NRF_ERROR_INVALID_PARAM;
    }

    p_cursor = cJSON_GetObjectItem(mp_config_json, CONFIG_PATH_KEY);
    if (p_cursor != NULL)
    {
        p_cursor = cJSON_GetObjectItem(p_cursor, (const char *)p_key);
        if ((p_cursor != NULL) && (p_cursor->type == cJSON_String))
        {
            memcpy(p_dst_str, p_cursor->valuestring, strlen(p_cursor->valuestring));
        }
        else
        {
            return NRF_ERROR_INVALID_PARAM;
        }
    }
    else
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    return NRF_SUCCESS;
}


/**@brief Function for parsing and processing config file.*/
static uint32_t app_dfu_config_process(iot_file_t              * p_file, 
                                       iot_dfu_firmware_desc_t * p_dfu_firmware_desc,
                                       char                    * p_filename)
{
    uint32_t err_code = NRF_SUCCESS;
    uint32_t app_size;
    uint32_t sd_size;
    uint32_t bl_size;
    uint32_t app_crc;
    uint32_t sd_crc;
    uint32_t bl_crc;
  
    // Clear global parameters before parsing a new one.
    memset(&m_firmware_desc, 0, sizeof(iot_dfu_firmware_desc_t));
    memset(p_filename, 0, MAX_LENGTH_FILENAME);
  
    mp_config_json = cJSON_Parse((const char *)m_config_mem);
    if (mp_config_json == NULL)
    {
        APPL_LOG("[APPL]: JSON parse failed.\r\n");
        return NRF_ERROR_INVALID_DATA;
    }

    check_bin((uint8_t *)(MBR_SIZE), CONFIG_SD_KEY, &sd_size, &sd_crc);
    check_bin((uint8_t *)(BOOTLOADER_REGION_START), CONFIG_BL_KEY, &bl_size, &bl_crc);
    check_bin((uint8_t *)(SD_SIZE_GET(MBR_SIZE)), CONFIG_APP_KEY, &app_size, &app_crc);

    if ((sd_size != 0) && (app_size != 0))
    {
        APPL_LOG("[APPL]: Update Softdevice with Application.\r\n");
      
        err_code = get_path(p_filename, CONFIG_APPSD_KEY);
        if (err_code == NRF_SUCCESS)
        {
            p_dfu_firmware_desc->softdevice.size  = sd_size;
            p_dfu_firmware_desc->softdevice.crc   = sd_crc;
            p_dfu_firmware_desc->application.size = app_size;
            p_dfu_firmware_desc->application.crc  = app_crc;
        }
    }
    else if (bl_size != 0)
    {
        APPL_LOG("[APPL]: Update Bootloader.\r\n");

        err_code = get_path(p_filename, CONFIG_BL_KEY);
        if (err_code == NRF_SUCCESS)
        {
            p_dfu_firmware_desc->bootloader.size = bl_size;
            p_dfu_firmware_desc->bootloader.crc  = bl_crc;
        }
    }
    else if (app_size != 0)
    {
        APPL_LOG("[APPL]: Update Application only.\r\n");

        err_code = get_path(p_filename, CONFIG_APP_KEY);
        if (err_code == NRF_SUCCESS)
        {
            p_dfu_firmware_desc->application.size = app_size;
            p_dfu_firmware_desc->application.crc  = app_crc;
        }
    }
    else
    {
        // This example application does not implement SoftDevice with Bootloader update
        APPL_LOG("[APPL]: Device firmware up to date.\r\n");
        err_code = NRF_ERROR_NOT_FOUND;
    }
    
    if(p_filename[0] == '\0')
    {
        APPL_LOG("[APPL]: File name has not be found.\r\n");
        err_code = NRF_ERROR_INVALID_PARAM;
    }

    cJSON_Delete(mp_config_json);
    
    return err_code;
}


/**@brief Function for handling IOT_DFU application events. 
 *
 * @note On IOT_DFU_WRITE_COMPLETE event TFTP transfer has to be resumed.
 *       The reason of this procedure is to allow asynchronous flash 
 *       write operation to be finished.
 */
static void app_iot_dfu_handler(uint32_t result, iot_dfu_evt_t event)
{
    uint32_t err_code;

    switch (event)
    {
        case IOT_DFU_WRITE_COMPLETE:
            // Resume TFTP transport, get another block.
            err_code = iot_tftp_resume(&m_tftp);
            APP_ERROR_CHECK(err_code);
            break;

        case IOT_DFU_ERROR:
            // Abort tftp transport.
            err_code = iot_tftp_abort(&m_tftp);
            APP_ERROR_CHECK(err_code);

            LEDS_OFF(LED_THREE);

            APPL_LOG("[APPL]: IoT DFU fails with result %08lx.\r\n", result);
            APP_ERROR_CHECK(result);
            break;

        default:
            break; 
    }
}


/**@brief Function for handling TFTP application events. */
static void app_tftp_handler(iot_tftp_t * p_tftp, iot_tftp_evt_t * p_evt)
{    
    uint32_t                err_code;
    iot_tftp_trans_params_t trans_params;

    APPL_LOG("[APPL]: In TFTP Application Handler.\r\n");

    if (p_evt->id == IOT_TFTP_EVT_ERROR)
    {
        APPL_LOG("[APPL]: TFTP error = 0x%08lx\r\n\t%s\r\n.", p_evt->param.err.code, p_evt->param.err.p_msg);

        m_dfu_state = APP_DFU_STATE_IDLE;

        // Set up timer for next config checking.
        err_code = app_timer_start(m_iot_dfu_timer_id, APP_DFU_POLLING_INTERVAL, NULL);
        APP_ERROR_CHECK(err_code);

        return;
    }

    if (p_evt->id == IOT_TFTP_EVT_TRANSFER_GET_COMPLETE)
    {
        APPL_LOG("[APPL]: TFTP transfer complete - size %ld.\r\n", p_evt->p_file->file_size);

        switch (m_dfu_state)
        {
            case APP_DFU_STATE_GET_CONFIG:
                APPL_LOG("[APPL]: Config file successfully downloaded.\r\n");

                m_config_mem[p_evt->p_file->file_size] = 0;
            
                err_code = app_dfu_config_process(p_evt->p_file, &m_firmware_desc, m_firmware_filename);
                if (err_code == NRF_SUCCESS)
                {
                    APPL_LOG("[APPL]: New sofware available. Starting downloading procedure.\r\n");

                    m_dfu_state = APP_DFU_STATE_GET_FIRMWARE;

                    mp_firmware_file->p_filename = m_firmware_filename;

                    trans_params.block_size = APP_TFTP_BLOCK_SIZE;
                    trans_params.next_retr  = APP_TFTP_RETRANSMISSION_TIME;

                    err_code = iot_tftp_set_params(&m_tftp, &trans_params);
                    APP_ERROR_CHECK(err_code);

                    err_code = iot_tftp_get(&m_tftp, mp_firmware_file);
                    APP_ERROR_CHECK(err_code);

                    LEDS_ON(LED_THREE);
                }
                else
                {
                    APPL_LOG("[APPL]: No new sofware available or incorrect JSON file.\r\n");

                    m_dfu_state = APP_DFU_STATE_IDLE;

                    // Set up timer for next config checking.
                    err_code = app_timer_start(m_iot_dfu_timer_id, APP_DFU_POLLING_INTERVAL, NULL);
                    APP_ERROR_CHECK(err_code);
                }
                break;

            case APP_DFU_STATE_GET_FIRMWARE:
                APPL_LOG("[APPL]: New firmware file successfully downloaded.\r\n");

                // Validate firmware image.
                err_code = iot_dfu_firmware_validate(&m_firmware_desc);
                APP_ERROR_CHECK(err_code);

                // Apply changes and swap firmware.
                err_code = iot_dfu_firmware_apply(&m_firmware_desc);
                APP_ERROR_CHECK(err_code);
                break;

            default:
                APP_ERROR_HANDLER(NRF_ERROR_INVALID_STATE);
                break;
        }
    }
}


/**@brief Function for handling the DFU configuration check timeout.
 *
 * @param[in] p_context The timeout context.
 */
static void app_dfu_timeout_handler(void * p_context)
{
    uint32_t                err_code;
    iot_tftp_trans_params_t trans_params;

    UNUSED_PARAMETER(p_context);

    if (m_dfu_state == APP_DFU_STATE_IDLE)
    {
        APPL_LOG("[APP_DFU]: Get new configuration file (timeout)...\r\n");

        m_dfu_state = APP_DFU_STATE_GET_CONFIG;
      
        trans_params.block_size = APP_TFTP_BLOCK_SIZE;
        trans_params.next_retr  = APP_TFTP_RETRANSMISSION_TIME;

        err_code = iot_tftp_set_params(&m_tftp, &trans_params);
        APP_ERROR_CHECK(err_code);

        err_code = iot_tftp_get(&m_tftp, &m_config_file);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling button events.
 *
 * @param[in]   pin_no         The pin number of the button pressed.
 * @param[in]   button_action  The action performed on button.
 */
static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
    uint32_t                err_code;
    iot_tftp_init_t         tftp_init_params;
    ipv6_addr_t             peer_addr;

    // Check if interface is UP.
    if (mp_interface == NULL)
    {
        return;
    }

    if (button_action == APP_BUTTON_PUSH)
    {
        switch (pin_no)
        {
            case START_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: DFU start button has been pushed.\r\n");

                // Initialize IOT_DFU module.              
                err_code = iot_dfu_init(app_iot_dfu_handler);
                APP_ERROR_CHECK(err_code);

                // Prepare file instance where the new image can be written to.
                err_code = iot_dfu_file_create(&mp_firmware_file);
                APP_ERROR_CHECK(err_code);
              
                // Initialize cJSON hooks for nRF.
                cJSON_Init();

                // Peer unicast address.
                IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&peer_addr, mp_interface->peer_addr.identifier);

                // Set TFTP configuration
                memset(&tftp_init_params, 0, sizeof(iot_tftp_init_t));
                tftp_init_params.p_ipv6_addr = &peer_addr;
                tftp_init_params.src_port    = APP_TFTP_LOCAL_PORT;
                tftp_init_params.dst_port    = APP_TFTP_SERVER_PORT;
                tftp_init_params.callback    = app_tftp_handler;

                // Initialize instance, bind socket, check parameters.
                err_code = iot_tftp_init(&m_tftp, &tftp_init_params);
                APP_ERROR_CHECK(err_code);

                // Initiliaze DFU config file abstraction.
                sprintf(m_config_filename, "/dfu/c/%d/%d",
                        DFU_DEVICE_INFO->device_type, DFU_DEVICE_INFO->device_rev);

                // Initialize file static instance.
                IOT_FILE_STATIC_INIT(&m_config_file,
                                     m_config_filename,
                                     m_config_mem,
                                     MAX_CONFIG_SIZE);

                // Start periodically check for the new firmware description.
                err_code = app_timer_start(m_iot_dfu_timer_id, APP_DFU_POLLING_INTERVAL, NULL);
                APP_ERROR_CHECK(err_code);

                break;
            }

            default:
                break;
        }
    }
}


/**@brief Function for the Button initialization.
 *
 * @details Initializes all Buttons used by this application.
 */
static void buttons_init(void)
{
    uint32_t err_code;

    static app_button_cfg_t buttons[] =
    {
        {START_BUTTON_PIN_NO, false, BUTTON_PULL, button_event_handler}
    };

    err_code = app_button_init(buttons, sizeof(buttons) / sizeof(buttons[0]), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for updating the wall clock of the IoT Timer module.
 */
static void iot_timer_tick_callback(void * p_context)
{
    UNUSED_VARIABLE(p_context);
    uint32_t err_code = iot_timer_update();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module, making it use the scheduler
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);

    // Initialize timer instance as a tick source for IoT timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,
                                APP_TIMER_MODE_REPEATED,
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
  
    // Starting the app timer instance for application part of dfu.
    err_code = app_timer_create(&m_iot_dfu_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                app_dfu_timeout_handler);
}


/**@brief Function for handling IPv6 application events.
 */
static void ip_app_handler(iot_interface_t * p_interface, ipv6_event_t * p_event)
{
    uint32_t    err_code;
    ipv6_addr_t src_addr;

    APPL_LOG("[APPL]: Got IP Application Handler Event on interface 0x%p\r\n", p_interface);

    switch (p_event->event_id)
    {
        case IPV6_EVT_INTERFACE_ADD:
#ifdef COMMISSIONING_ENABLED
            commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_STOP_RESET);
#endif // COMMISSIONING_ENABLED

            APPL_LOG("[APPL]: New interface added!\r\n");
            mp_interface = p_interface;

            m_display_state = LEDS_IPV6_IF_UP;

            APPL_LOG("[APPL]: Sending Router Solicitation to all routers!\r\n");

            // Create Link Local addresses
            IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&src_addr, p_interface->local_addr.identifier);

            // Delay first solicitation due to possible restriction on other side.
            nrf_delay_ms(APP_RTR_SOLICITATION_DELAY);

            // Send Router Solicitation to all routers.
            err_code = icmp6_rs_send(p_interface,
                                     &src_addr,
                                     &m_local_routers_multicast_addr);
            APP_ERROR_CHECK(err_code);
            break;

        case IPV6_EVT_INTERFACE_DELETE:
#ifdef COMMISSIONING_ENABLED
            commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED

            APPL_LOG("[APPL]: Interface removed!\r\n");
            mp_interface = NULL;
            m_display_state = LEDS_IPV6_IF_DOWN;

            break;

        case IPV6_EVT_INTERFACE_RX_DATA:
            APPL_LOG("[APPL]: Got unsupported protocol data!\r\n");
            break;

        default:
            // Unknown event. Should not happen.
            break;
    }
}


/**@brief Function for initializing IP stack.
 *
 * @details Initialize the IP Stack.
 */
static void ip_stack_init(void)
{
    uint32_t    err_code;
    ipv6_init_t init_param;

    err_code = ipv6_medium_eui64_get(m_ipv6_medium.ipv6_medium_instance_id, \
                                     &eui64_local_iid);
    APP_ERROR_CHECK(err_code);

    init_param.p_eui64       = &eui64_local_iid;
    init_param.event_handler = ip_app_handler;

    err_code = ipv6_init(&init_param);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the IoT Timer. */
static void ip_stack_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {blink_timeout_handler,    LED_BLINK_INTERVAL_MS},
        {iot_tftp_timeout_process, 3000},
#ifdef COMMISSIONING_ENABLED
        {commissioning_time_tick,  SEC_TO_MILLISEC(COMMISSIONING_TICK_INTERVAL_SEC)}
#endif // COMMISSIONING_ENABLED
    };

    // The list of IoT Timer clients is declared as a constant.
    static const iot_timer_clients_list_t iot_timer_clients =
    {
        (sizeof(list_of_clients) / sizeof(iot_timer_client_t)),
        &(list_of_clients[0]),
    };

    // Passing the list of clients to the IoT Timer module.
    err_code = iot_timer_client_list_set(&iot_timer_clients);
    APP_ERROR_CHECK(err_code);

    // Starting the app timer instance that is the tick source for the IoT Timer.
    err_code = app_timer_start(m_iot_timer_tick_src_id,
                               APP_TIMER_TICKS(IOT_TIMER_RESOLUTION_IN_MS, APP_TIMER_PRESCALER),
                               NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();

    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting connectable mode.
 */
static void connectable_mode_enter(void)
{
    uint32_t err_code = ipv6_medium_connectable_mode_enter(m_ipv6_medium.ipv6_medium_instance_id);
    APP_ERROR_CHECK(err_code);

    APPL_LOG("[APPL]: Physical layer in connectable mode.\r\n");
    m_display_state = LEDS_CONNECTABLE_MODE;
}


static void on_ipv6_medium_evt(ipv6_medium_evt_t * p_ipv6_medium_evt)
{
    switch (p_ipv6_medium_evt->ipv6_medium_evt_id)
    {
        case IPV6_MEDIUM_EVT_CONN_UP:
        {
            APPL_LOG("[APPL]: Physical layer: connected.\r\n");
            m_display_state = LEDS_IPV6_IF_DOWN;
            break;
        }
        case IPV6_MEDIUM_EVT_CONN_DOWN:
        {
            APPL_LOG("[APPL]: Physical layer: disconnected.\r\n");
            connectable_mode_enter();
            break;
        }
        default:
        {
            break;
        }
    }
}


static void on_ipv6_medium_error(ipv6_medium_error_t * p_ipv6_medium_error)
{
    // Do something.
}


#ifdef COMMISSIONING_ENABLED
void commissioning_id_mode_cb(mode_control_cmd_t control_command)
{
    switch (control_command)
    {
        case CMD_IDENTITY_MODE_ENTER:
        {
            LEDS_OFF(LED_THREE | LED_FOUR);
            m_identity_mode_active = true;

            break;
        }
        case CMD_IDENTITY_MODE_EXIT:
        {
            m_identity_mode_active = false;
            LEDS_OFF((LED_THREE | LED_FOUR));

            break;
        }
        default:
        {
            
            break;
        }
    }
}


void commissioning_power_off_cb(bool power_off_on_failure)
{
    m_power_off_on_failure = power_off_on_failure;

    APPL_LOG("[APPL]: Commissioning: do power_off on failure: %s.\r\n", \
             m_power_off_on_failure ? "true" : "false");
}
#endif // COMMISSIONING_ENABLED


/**@brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;

    // Initialize
    app_trace_init();
    leds_init();
    timers_init();
    buttons_init();

    static ipv6_medium_init_params_t ipv6_medium_init_params;
    memset(&ipv6_medium_init_params, 0x00, sizeof(ipv6_medium_init_params));
    ipv6_medium_init_params.ipv6_medium_evt_handler    = on_ipv6_medium_evt;
    ipv6_medium_init_params.ipv6_medium_error_handler  = on_ipv6_medium_error;
    ipv6_medium_init_params.use_scheduler              = false;
#ifdef COMMISSIONING_ENABLED
    ipv6_medium_init_params.commissioning_id_mode_cb   = commissioning_id_mode_cb;
    ipv6_medium_init_params.commissioning_power_off_cb = commissioning_power_off_cb;
#endif // COMMISSIONING_ENABLED

    err_code = ipv6_medium_init(&ipv6_medium_init_params, \
                                IPV6_MEDIUM_ID_BLE,       \
                                &m_ipv6_medium);
    APP_ERROR_CHECK(err_code);

    eui48_t ipv6_medium_eui48;
    err_code = ipv6_medium_eui48_get(m_ipv6_medium.ipv6_medium_instance_id, \
                                     &ipv6_medium_eui48);

    ipv6_medium_eui48.identifier[EUI_48_SIZE - 1] = 0x00;

    err_code = ipv6_medium_eui48_set(m_ipv6_medium.ipv6_medium_instance_id, \
                                     &ipv6_medium_eui48);
    APP_ERROR_CHECK(err_code);

    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    ip_stack_init();
    ip_stack_timer_init();

    APPL_LOG("\r\n");
    APPL_LOG("[APPL]: Init complete.\r\n");

    // Start execution
    connectable_mode_enter();

    // Enter main loop
    for (;;)
    {
        power_manage();
    }
}


/**
 * @}
 */
