/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
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
 * @defgroup iot_sdk_app_xively_mqtt_client main.c
 * @{
 * @ingroup iot_sdk_app_lwip
 *
 * @brief This file contains the source code for LwIP based MQTT Client sample application
 *        that uses Xively cloud service. Example regularly publishes a simulated temperature
 *        into Xively cloud.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "boards.h"
#include "app_timer_appsh.h"
#include "app_scheduler.h"
#include "app_button.h"
#include "nordic_common.h"
#include "softdevice_handler_appsh.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "ble_6lowpan.h"
#include "mem_manager.h"
#include "app_trace.h"
#include "lwip/init.h"
#include "lwip/inet6.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"
#include "mqtt.h"
#include "lwip/timers.h"
#include "iot_timer.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"
#include "ipv6_medium.h"
#include "sdk_config.h"

#define LED_ONE                             BSP_LED_0_MASK
#define LED_TWO                             BSP_LED_1_MASK
#define LED_THREE                           BSP_LED_2_MASK
#define LED_FOUR                            BSP_LED_3_MASK
#define ALL_APP_LED                        (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                            BSP_LED_2_MASK | BSP_LED_3_MASK)                        /**< Define used for simultaneous operation of all application LEDs. */

#ifdef COMMISSIONING_ENABLED
#define ERASE_BUTTON_PIN_NO                 BSP_BUTTON_3                                            /**< Button used to erase commissioning settings. */
#endif // COMMISSIONING_ENABLED

#define APP_TIMER_OP_QUEUE_SIZE             5
#define APP_XIVELY_CONNECT_DELAY            APP_TIMER_TICKS(500, APP_TIMER_PRESCALER)               /**< MQTT Connection delay. */

#define LWIP_SYS_TICK_MS                    10                                                      /**< Interval for timer used as trigger to send. */
#define LED_BLINK_INTERVAL_MS               300                                                     /**< LED blinking interval. */
#define MQTT_PUBLISH_INTERVAL_MS            10000                                                   /**< Interval between publishing data. */

#define DEAD_BEEF                           0xDEADBEEF                                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME                 128                                                     /**< Max length of filename to copy for the debug error handler. */

#define APPL_LOG                            app_trace_log                                           /**< Macro for logging application messages on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */
#define APPL_DUMP                           app_trace_dump                                          /**< Macro for dumping application data on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */

#define APP_DATA_INITIAL_TEMPERATURE        17                                                      /**< Initial simulated temperature. */
#define APP_DATA_MAXIMUM_TEMPERATURE        25                                                      /**< Maximum simulated temperature. */
#define APP_DATA_MAX_SIZE                   200                                                     /**< Maximum size of data buffer size. */

#define APP_MQTT_XIVELY_BROKER_PORT         8883                                                    /**< Port number of MQTT Broker. */
#define APP_MQTT_XIVELY_CHANNEL             "Thermometer"                                           /**< Device's Channel. */
#define APP_MQTT_XIVELY_FEED_ID             "{PUT FEED ID HERE}"                                    /**< Device's Feed ID. */
#define APP_MQTT_XIVELY_API_KEY             "{PUT API KEY HERE}"                                    /**< API key used for authentication. */

// URL of given Xively JSON resource.
#define APP_MQTT_XIVELY_API_URL             "/v2/feeds/" APP_MQTT_XIVELY_FEED_ID ".json"

// Xively data format.
#define APP_MQTT_XIVELY_DATA_FORMAT         "{"                                                \
                                                "\"datastreams\":["                            \
                                                    "{\"id\":\"" APP_MQTT_XIVELY_CHANNEL "\"," \
                                                    "\"current_value\":\"%d.00\"}"             \
                                                "]"                                            \
                                             "}"

typedef enum
{
    LEDS_INACTIVE = 0,
    LEDS_CONNECTABLE_MODE,
    LEDS_IPV6_IF_DOWN,
    LEDS_IPV6_IF_UP,
    LEDS_CONNECTED_TO_BROKER,
    LEDS_PUBLISHING
} display_state_t;

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                             /**< App timer instance used to update the IoT timer wall clock. */
APP_TIMER_DEF(m_mqtt_conn_timer_id);                                                                /**< Timer for delaying MQTT Connection. */
eui64_t                                     eui64_local_iid;                                        /**< Local EUI64 value that is used as the IID for*/
static ipv6_medium_instance_t               m_ipv6_medium;
static display_state_t                      m_display_state    = LEDS_INACTIVE;                     /**< Board LED display state. */
static bool                                 m_interface_state  = false;                             /**< Actual status of interface. */
static bool                                 m_connection_state = false;                             /**< MQTT Connection state. */
static bool                                 m_do_publication   = false;                             /**< Indicates if MQTT publications are enabled. */

static mqtt_client_t                        m_app_mqtt_id;                                          /**< MQTT Client instance reference provided by the MQTT module. */
static const char                           m_device_id[]      = "nrfPublisher";                    /**< Unique MQTT client identifier. */
static const char                           m_user[]           = APP_MQTT_XIVELY_API_KEY;           /**< MQTT user name. */
static uint16_t                             m_temperature      = APP_DATA_INITIAL_TEMPERATURE;      /**< Actual simulated temperature. */
static char                                 m_data_body[APP_DATA_MAX_SIZE];                         /**< Buffer used for publishing data. */
static uint16_t                             m_message_counter = 1;                                  /**< Message counter used to generated message ids for MQTT messages. */

// TLS settings.
static nrf_tls_key_settings_t m_tls_keys =
{
    .p_psk             = NULL, // PSK not used.
    .p_ca_cert_pem     = NULL, // No certificate verification.
    .p_own_certificate = NULL  // Use server certificiate.
};

// Address of Xively cloud.
static const uint8_t                        m_broker_addr[IPV6_ADDR_SIZE] =
{
       0x20, 0x01, 0x07, 0x78,
       0x00, 0x00, 0xff, 0xff,
       0x00, 0x64, 0x00, 0x00,
       0xd8, 0x34, 0xe9, 0x7a
};

#ifdef COMMISSIONING_ENABLED
static bool                                 m_power_off_on_failure = false;
static bool                                 m_identity_mode_active;
#endif // COMMISSIONING_ENABLED

/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
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
    // NVIC_SystemReset();
    for (;;)
    {
    }
#endif // COMMISSIONING_ENABLED
}


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
#define MAX_LENGTH_FILENAME                 128                                                     /**< Max length of filename to copy for the debug error handler. */
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by this application.
 */
static void leds_init(void)
{
    // Configure application LED pins.
    LEDS_CONFIGURE(ALL_APP_LED);

    // Turn off all LED on initialization.
    LEDS_OFF(ALL_APP_LED);
}


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
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
            LEDS_OFF(ALL_APP_LED);
            break;
        }
        case LEDS_CONNECTABLE_MODE:
        {
            LEDS_INVERT(LED_ONE);
            LEDS_OFF((LED_TWO | LED_THREE | LED_FOUR));
            break;
        }
        case LEDS_IPV6_IF_DOWN:
        {
            LEDS_INVERT(LED_TWO);
            LEDS_OFF((LED_ONE | LED_THREE | LED_FOUR));
            break;
        }
        case LEDS_IPV6_IF_UP:
        {
            LEDS_ON(LED_ONE);
            LEDS_OFF((LED_TWO | LED_THREE | LED_FOUR));
            break;
        }
        case LEDS_CONNECTED_TO_BROKER:
        {
            LEDS_ON(LED_TWO);
            LEDS_OFF((LED_ONE | LED_THREE | LED_FOUR));
            m_display_state = LEDS_PUBLISHING;
            break;
        }
        case LEDS_PUBLISHING:
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
#ifdef COMMISSIONING_ENABLED
    if ((button_action == APP_BUTTON_PUSH) && (pin_no == ERASE_BUTTON_PIN_NO))
    {
        APPL_LOG("[APPL]: Erasing all commissioning settings from persistent storage... \r\n");
        commissioning_settings_clear();
        return;
    }
#endif // COMMISSIONING_ENABLED

    uint32_t err_code;

    // Check if interface has been established.
    if(m_interface_state == false)
    {
        return;
    }

    if (button_action == APP_BUTTON_PUSH)
    {
        switch (pin_no)
        {
            case BSP_BUTTON_0:
            {
                if (m_connection_state == false)
                {
                    err_code = app_timer_start(m_mqtt_conn_timer_id, APP_XIVELY_CONNECT_DELAY, NULL);
                    APP_ERROR_CHECK(err_code);
                }
                break;
            }
            case BSP_BUTTON_1:
            {
                if (m_connection_state == true)
                {
                    err_code = mqtt_disconnect(&m_app_mqtt_id);
                    APP_ERROR_CHECK(err_code);
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
static void button_init(void)
{
    uint32_t err_code;

    static app_button_cfg_t buttons[] =
    {
        {BSP_BUTTON_0,        false, BUTTON_PULL, button_event_handler},
        {BSP_BUTTON_1,        false, BUTTON_PULL, button_event_handler},
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


/**@brief Asynchronous event notification callback registered by the application with the module to receive MQTT events. */
static void app_mqtt_evt_handler(mqtt_client_t * p_client, const mqtt_evt_t * p_evt)
{
    uint32_t err_code;
    switch(p_evt->id)
    {
        case MQTT_EVT_CONNECT:
        {
            APPL_LOG("[APPL]: >> MQTT_EVT_CONNECT, result: 0x%08x\r\n", p_evt->result);

            if(p_evt->result == NRF_SUCCESS)
            {
                m_connection_state = true;
                m_do_publication   = true;
                m_display_state = LEDS_CONNECTED_TO_BROKER;
            }
            break;
        }
        case MQTT_EVT_DISCONNECT:
        {
            APPL_LOG ("[APPL]: >> MQTT_EVT_DISCONNECT\r\n");
            m_connection_state = false;
            m_display_state = LEDS_IPV6_IF_UP;

            // Make reconnection in case of tcp problem.
            if(p_evt->result == MQTT_ERR_TRANSPORT_CLOSED)
            {
                err_code = app_timer_start(m_mqtt_conn_timer_id, APP_XIVELY_CONNECT_DELAY, NULL);
                APP_ERROR_CHECK(err_code);
            }
            else
            {
                // In case of normal MQTT disconnection, turn off data transfer.
                m_do_publication = false;
            }

            break;
        }
        default:
            break;
    }
}


/**@brief Timer callback used for publishing simulated temperature periodically.
 *
 */
static void app_xively_publish_callback(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);
    uint32_t err_code;

    // Check if data transfer is enabled.
    if (m_do_publication == false)
    {
        return;
    }

    // Check if connection has been established.
    if (m_connection_state == false)
    {
        return;
    }

    // Reset temperature to initial state.
    if (m_temperature == APP_DATA_MAXIMUM_TEMPERATURE)
    {
        m_temperature = APP_DATA_INITIAL_TEMPERATURE;
    }

    // Prepare data in JSON format.
    sprintf(m_data_body, APP_MQTT_XIVELY_DATA_FORMAT, m_temperature);
    
    mqtt_publish_param_t param;
    
    param.message.topic.qos              = MQTT_QoS_0_AT_MOST_ONCE;
    param.message.topic.topic.p_utf_str  = (uint8_t *)APP_MQTT_XIVELY_API_URL;
    param.message.topic.topic.utf_strlen = strlen(APP_MQTT_XIVELY_API_URL);
    param.message.payload.p_bin_str      = (uint8_t *)m_data_body;
    param.message.payload.bin_strlen     = strlen(m_data_body);
    param.message_id                     = m_message_counter;
    param.dup_flag                       = 0;
    param.retain_flag                    = 0;

    APPL_LOG("[APPL]: Publishing value of %02d.00\r\n", m_temperature);

    // Publish data.
    err_code = mqtt_publish(&m_app_mqtt_id, &param);

    // Check if there is no pending transaction.
    if(err_code != (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE))
    {
        APP_ERROR_CHECK(err_code);
        m_message_counter += 2;
    }

    // Change leds to indicate actual data that has been sent.
    switch(m_temperature % 4)
    {
      case 0:
        LEDS_OFF(LED_THREE | LED_FOUR);
        break;
      case 1:
        LEDS_ON(LED_FOUR);
        LEDS_OFF(LED_THREE);
        break;
      case 2:
        LEDS_ON(LED_THREE);
        LEDS_OFF(LED_FOUR);
        break;
      case 3:
        LEDS_ON(LED_THREE | LED_FOUR);
        break;
    }

    // Increase temperature value.
    m_temperature++;

    APPL_LOG("[APPL]: mqtt_publish result 0x%08lx\r\n", err_code);
}


/**@brief Timer callback used for connecting to Xively cloud.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void app_xively_connect(void * p_context)
{
    UNUSED_VARIABLE(p_context);

    uint32_t       err_code;
    
    mqtt_client_init(&m_app_mqtt_id);

    static mqtt_username_t username;
    username.p_utf_str  = (uint8_t *)m_user;
    username.utf_strlen = strlen(m_user);

    memcpy (m_app_mqtt_id.broker_addr.u8, m_broker_addr, IPV6_ADDR_SIZE);
    
    m_app_mqtt_id.broker_port             = APP_MQTT_XIVELY_BROKER_PORT;
    m_app_mqtt_id.evt_cb                  = app_mqtt_evt_handler;
    m_app_mqtt_id.client_id.p_utf_str     = (uint8_t *)m_device_id;
    m_app_mqtt_id.client_id.utf_strlen    = strlen(m_device_id);
    m_app_mqtt_id.p_password              = NULL;
    m_app_mqtt_id.p_user_name             = (mqtt_username_t *)&username;
    m_app_mqtt_id.transport_type          = MQTT_TRANSPORT_SECURE;
    m_app_mqtt_id.p_security_settings     = &m_tls_keys;

    err_code = mqtt_connect(&m_app_mqtt_id);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing IP stack.
 *
 * @details Initialize the IP Stack and its driver.
 */
static void ip_stack_init(void)
{
    uint32_t err_code;

    err_code = ipv6_medium_eui64_get(m_ipv6_medium.ipv6_medium_instance_id, \
                                     &eui64_local_iid);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_mem_init();
    APP_ERROR_CHECK(err_code);

    //Initialize LwIP stack.
    lwip_init();

    //Initialize LwIP stack driver.
    err_code = nrf_driver_init();
    APP_ERROR_CHECK(err_code);

    err_code = mqtt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Timer callback used for periodic servicing of LwIP protocol timers.
 *        This trigger is also used in the example to trigger sending TCP Connection.
 *
 * @details Timer callback used for periodic servicing of LwIP protocol timers.
 *
 */
static void app_lwip_time_tick(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);
    sys_check_timeouts();
    UNUSED_VARIABLE(mqtt_live());
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
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module.
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create timer to create delay connection to Xively.
    err_code = app_timer_create(&m_mqtt_conn_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                app_xively_connect);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {app_lwip_time_tick,          LWIP_SYS_TICK_MS},
        {blink_timeout_handler,       LED_BLINK_INTERVAL_MS},
        {app_xively_publish_callback, MQTT_PUBLISH_INTERVAL_MS},
#ifdef COMMISSIONING_ENABLED
        {commissioning_time_tick,     SEC_TO_MILLISEC(COMMISSIONING_TICK_INTERVAL_SEC)}
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

    // Create app timer to serve as tick source.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,
                                APP_TIMER_MODE_REPEATED,
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
    
    // Starting the app timer instance that is the tick source for the IoT Timer.
    err_code = app_timer_start(m_iot_timer_tick_src_id, \
                               APP_TIMER_TICKS(IOT_TIMER_RESOLUTION_IN_MS, APP_TIMER_PRESCALER), \
                               NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function to handle interface up event. */
void nrf_driver_interface_up(void)
{
    APPL_LOG ("[APPL]: IPv6 Interface Up.\r\n");
#ifdef COMMISSIONING_ENABLED
    commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_STOP_RESET);
#endif // COMMISSIONING_ENABLED

    sys_check_timeouts();

    // Set flag indicating interface state.
    m_interface_state = true;
    m_display_state = LEDS_IPV6_IF_UP;
}


/**@brief Function to handle interface down event. */
void nrf_driver_interface_down(void)
{
    APPL_LOG ("[APPL]: IPv6 Interface Down.\r\n");
#ifdef COMMISSIONING_ENABLED
    commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED

    // Clear flag indicating interface state.
    m_interface_state = false;
    m_display_state = LEDS_IPV6_IF_DOWN;

    // A system reset is issued, becuase a defect 
    // in lwIP prevents recovery -- see SDK release notes for details.
    NVIC_SystemReset();
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


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;

    //Initialize.
    app_trace_init();
    leds_init();
    timers_init();
    iot_timer_init();
    button_init();

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

    //Start execution.
    connectable_mode_enter();

    //Enter main loop.
    for (;;)
    {
        //Sleep waiting for an application event.
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

/**
 * @}
 */
