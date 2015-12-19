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
 * @defgroup iot_sdk_tftp_app main.c
 * @{
 * @ingroup iot_sdk_app_ipv6
 * @brief This file contains the source code for Nordic's IPv6 TFTP client sample application.
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
#include "iot_common.h"
#include "iot_timer.h"
#include "ipv6_api.h"
#include "icmp6_api.h"

#include "iot_file.h"
#include "iot_file_static.h"
#include "iot_tftp.h"
#include "ipv6_medium.h"

#define LED_ONE                         BSP_LED_0_MASK
#define LED_TWO                         BSP_LED_1_MASK
#define LED_THREE                       BSP_LED_2_MASK
#define LED_FOUR                        BSP_LED_3_MASK

#define GET_BUTTON_PIN_NO               BSP_BUTTON_0                                                /**< Button used to start application state machine. */
#define PUT_BUTTON_PIN_NO               BSP_BUTTON_1                                                /**< Button used to stop application state machine. */

#ifdef COMMISSIONING_ENABLED
#define ERASE_BUTTON_PIN_NO             BSP_BUTTON_3                                                /**< Button used to erase commissioning settings. */
#endif // COMMISSIONING_ENABLED

#define APP_TIMER_OP_QUEUE_SIZE         5                                                           /**< Size of timer operation queues. */

#define LED_BLINK_INTERVAL_MS           500                                                         /**< LED blinking interval. */
#define APP_RTR_SOLICITATION_DELAY      1000                                                        /**< Time before host sends an initial solicitation in ms. */

#define DEAD_BEEF                       0xDEADBEEF                                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define ERR_MAX_LENGTH_FN               128                                                         /**< Max length of filename to copy for the debug error handler. */

#define APP_ENABLE_LOGS                 1                                                           /**< Disable debug trace in the application. */
#define MAX_LENGTH_FILENAME             256                                                         /**< Maximum filename size */
#define MAX_FILE_SIZE                   512                                                         /**< Maximum file size. */

#define APP_TFTP_USER_STRING            " Hello! "                                                  /**< String that is added on PUT operation. */
#define APP_TFTP_FILE_NAME              "test.txt"                                                  /**< Name of file that is used in TFTP operation. */

#define APP_TFTP_LOCAL_PORT             100                                                         /**< Local UDP port for TFTP client usage. */
#define APP_TFTP_SERVER_PORT            69                                                          /**< UDP port on which TFTP server listens. */
#define APP_TFTP_BLOCK_SIZE             64                                                          /**< Maximum or negotiated size of data block. */
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

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                             /**< App timer instance used to update the IoT timer wall clock. */

static ipv6_medium_instance_t   m_ipv6_medium;
eui64_t                         eui64_local_iid;                                                    /**< Local EUI64 value that is used as the IID for*/
static iot_interface_t        * mp_interface = NULL;                                                /**< Pointer to IoT interface if any. */
static display_state_t          m_display_state = LEDS_INACTIVE;                                    /**< Board LED display state. */

static uint8_t                  m_static_memory[MAX_FILE_SIZE];                                     /**< Static memory for IoT File. */
static iot_file_t               m_file;                                                             /**< IoT File for static buffers. */
static iot_tftp_t               m_tftp;                                                             /**< TFTP client instance. */

/**@brief Addresses used in sample application. */
static const ipv6_addr_t        m_local_routers_multicast_addr = {{0xFF, 0x02, 0x00, 0x00, \
                                                                   0x00, 0x00, 0x00, 0x00, \
                                                                   0x00, 0x00, 0x00, 0x00, \
                                                                   0x00, 0x00, 0x00, 0x02}};        /**< Multicast address of all routers on the local network segment. */

#ifdef COMMISSIONING_ENABLED
static bool                     m_power_off_on_failure = false;
static bool                     m_identity_mode_active;
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
    LEDS_ON((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
    APPL_LOG("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s\r\n", \
             error_code, line_num, p_file_name);

    // Variables used to retain function parameter values when debugging.
    static volatile uint8_t  s_file_name[ERR_MAX_LENGTH_FN];
    static volatile uint16_t s_line_num;
    static volatile uint32_t s_error_code;

    strncpy((char *)s_file_name, (const char *)p_file_name, ERR_MAX_LENGTH_FN - 1);
    s_file_name[ERR_MAX_LENGTH_FN - 1]   = '\0';
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

    if (m_identity_mode_active == false)
    {
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
        {
            break;
        }
    }
#ifdef COMMISSIONING_ENABLED
    }
#endif // COMMISSIONING_ENABLED

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


/**@brief Function for handling button events.
 *
 * @param[in]   pin_no         The pin number of the button pressed.
 * @param[in]   button_action  The action performed on button.
 */
static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
    uint32_t err_code;
#ifdef COMMISSIONING_ENABLED
    if ((button_action == APP_BUTTON_PUSH) && (pin_no == ERASE_BUTTON_PIN_NO))
    {
        APPL_LOG("[APPL]: Erasing all commissioning settings from persistent storage... \r\n");
        commissioning_settings_clear();
        return;
    }
#endif // COMMISSIONING_ENABLED

    // Check if interface is UP.
    if(mp_interface == NULL)
    {
        return;
    }

    if (button_action == APP_BUTTON_PUSH)
    {
        switch (pin_no)
        {
            case GET_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: Read button has been pushed.\r\n");

                // Get file from the server.
                err_code = iot_tftp_get(&m_tftp, &m_file);

                if(err_code != NRF_SUCCESS)
                {
                    APPL_LOG("[APPL]: Operation GET failed, reason %08lx.\r\n", err_code);
                }

                break;
            }
            case PUT_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: Write button has been pushed.\r\n");

                // Append APP_TFTP_USER_STRING string at the end of the file.
                err_code = iot_file_fopen(&m_file, 0);
                APP_ERROR_CHECK(err_code);

                err_code = iot_file_fseek(&m_file, m_file.file_size);
                APP_ERROR_CHECK(err_code);

                err_code = iot_file_fwrite(&m_file, APP_TFTP_USER_STRING, strlen(APP_TFTP_USER_STRING));
                APP_ERROR_CHECK(err_code);

                err_code = iot_file_fclose(&m_file);
                APP_ERROR_CHECK(err_code);

                // Send modified file to the server.
                err_code = iot_tftp_put(&m_tftp, &m_file);

                if(err_code != NRF_SUCCESS)
                {
                    APPL_LOG("[APPL]: Operation PUT failed, reason %08lx.\r\n", err_code);
                }

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
        {GET_BUTTON_PIN_NO,   false, BUTTON_PULL, button_event_handler},
        {PUT_BUTTON_PIN_NO,   false, BUTTON_PULL, button_event_handler},
#ifdef COMMISSIONING_ENABLED
        {ERASE_BUTTON_PIN_NO, false, BUTTON_PULL, button_event_handler}
#endif // COMMISSIONING_ENABLED
    };

    #define BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)

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
}


/**@brief Function for handling TFTP application events. */
void app_tftp_handler(iot_tftp_t * p_tftp, iot_tftp_evt_t * p_evt)
{
    char     str[MAX_FILE_SIZE];
    uint32_t err_code;

    APPL_LOG("[APPL]: In TFTP Application Handler.\r\n");

    switch(p_evt->id)
    {
        case IOT_TFTP_EVT_ERROR:
            APPL_LOG("[APPL]: TFTP error = 0x%08lx\r\n\t%s\r\n. Transfered %ld bytes.", p_evt->param.err.code,
                     p_evt->param.err.p_msg, p_evt->param.err.size_transfered);
        break;

        case IOT_TFTP_EVT_TRANSFER_GET_COMPLETE:
        case IOT_TFTP_EVT_TRANSFER_PUT_COMPLETE:
            APPL_LOG("[APPL]: TFTP transfer complete size: %ld \r\n", m_file.file_size);

            memset(str, 0, sizeof(char));

            err_code = iot_file_fopen(&m_file, 0);
            APP_ERROR_CHECK(err_code);

            err_code = iot_file_frewind(&m_file);
            APP_ERROR_CHECK(err_code);

            err_code = iot_file_fread(&m_file, str, m_file.file_size);
            APP_ERROR_CHECK(err_code);

            // Ensure string ends with right terminator.
            str[m_file.file_size] = '\0';

            APPL_LOG("[APPL]: Content of the file: \r\n%s\r\n", str);
        break;

        default:
            break;
    }
}


/**@brief Function for initializing TFTP client. */
void app_tftp_init(void)
{
    uint32_t                err_code;
    iot_tftp_init_t         tftp_init_params;
    iot_tftp_trans_params_t trans_params;
    ipv6_addr_t             peer_addr;

    IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&peer_addr, mp_interface->peer_addr.identifier);

    // Set TFTP configuration.
    memset(&tftp_init_params, 0, sizeof(iot_tftp_init_t));
    tftp_init_params.p_ipv6_addr = &peer_addr;
    tftp_init_params.src_port    = APP_TFTP_LOCAL_PORT;
    tftp_init_params.dst_port    = APP_TFTP_SERVER_PORT;
    tftp_init_params.callback    = app_tftp_handler;

    // Initialize instance, bind socket, check parameters.
    err_code = iot_tftp_init(&m_tftp, &tftp_init_params);
    APP_ERROR_CHECK(err_code);

    // Set initial connection parameters. Note that they could be modified by negotiating procedure,
    //   but each transfer will reset to initial parameters configured by set_params() function.
    trans_params.block_size = APP_TFTP_BLOCK_SIZE;
    trans_params.next_retr  = APP_TFTP_RETRANSMISSION_TIME;

    // Set initial connection parameters.
    err_code = iot_tftp_set_params(&m_tftp, &trans_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling IPv6 application events. */
static void ip_app_handler(iot_interface_t * p_interface, ipv6_event_t * p_event)
{
    uint32_t                err_code;
    ipv6_addr_t             src_addr;

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

            // Create Link Local address.
            IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&src_addr, p_interface->local_addr.identifier);

            // Delay first solicitation due to possible restriction on other side.
            nrf_delay_ms(APP_RTR_SOLICITATION_DELAY);

            // Send Router Solicitation to all routers.
            err_code = icmp6_rs_send(p_interface,
                                     &src_addr,
                                     &m_local_routers_multicast_addr);
            APP_ERROR_CHECK(err_code);

            // Initialize TFTP transfer.
            app_tftp_init();
        break;

        case IPV6_EVT_INTERFACE_DELETE:
#ifdef COMMISSIONING_ENABLED
            commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED
            APPL_LOG("[APPL]: Interface removed!\r\n");
            mp_interface = NULL;

            m_display_state = LEDS_IPV6_IF_DOWN;

            // Initialize instance, bind socket, check parameters.
            err_code = iot_tftp_uninit(&m_tftp);
            APP_ERROR_CHECK(err_code);

            break;

        case IPV6_EVT_INTERFACE_RX_DATA:
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

    // Clear memory used for file abstraction.
    memset(m_static_memory, 0, sizeof(m_static_memory));

    // Initialize IoT Static File.
    IOT_FILE_STATIC_INIT(&m_file, APP_TFTP_FILE_NAME, m_static_memory, sizeof(m_static_memory));
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {blink_timeout_handler,    LED_BLINK_INTERVAL_MS},
        {iot_tftp_timeout_process, TFTP_RETRANSMISSION_TIMER_INTERVAL},
#ifdef COMMISSIONING_ENABLED
        {commissioning_time_tick, SEC_TO_MILLISEC(COMMISSIONING_TICK_INTERVAL_SEC)}
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
    err_code = app_timer_start(m_iot_timer_tick_src_id, \
                               APP_TIMER_TICKS(IOT_TIMER_RESOLUTION_IN_MS, APP_TIMER_PRESCALER), \
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

    // Initialize.
    app_trace_init();
    leds_init();
    timers_init();
    iot_timer_init();
    buttons_init();

#ifdef COMMISSIONING_ENABLED
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);
#endif // COMMISSIONING_ENABLED

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

    ip_stack_init();

    APPL_LOG("\r\n");
    APPL_LOG("[APPL]: Init complete.\r\n");

    // Start execution.
    connectable_mode_enter();

    // Enter main loop.
    for (;;)
    {
        power_manage();
    }
}


/**
 * @}
 */
