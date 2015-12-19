/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
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
 * @defgroup ble_sdk_app_template_main main.c
 * @{
 * @ingroup ble_sdk_app_template
 * @brief Template project main file.
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "app_trace.h"
#include "app_timer_appsh.h"
#include "app_button.h"
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
#include "lwip/timers.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"
#include "iot_timer.h"
#include "coap_api.h"
#include "coap_observe_api.h"
#include "ipv6_medium.h"

/** Modify SERVER_IPV6_ADDRESS according to your setup.
 *  The address provided below is a place holder.  */
#define SERVER_IPV6_ADDRESS             0x20, 0x01, 0x0D, 0xB8, 0x00, 0x00, 0x00, 0x00, \
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01        /**< IPv6 address of the server node. */

// The CoAP default port number 5683 MUST be supported by a server.
#define LOCAL_PORT_NUM                  5683                                                  /**< CoAP default port number. */
#define REMOTE_PORT_NUM                 5683                                                  /**< Remote port number. */

#define COAP_MESSAGE_TYPE               COAP_TYPE_NON                                         /**< Message type for all outgoing CoAP requests. */

#define LED_ONE                         BSP_LED_0_MASK
#define LED_TWO                         BSP_LED_1_MASK
#define LED_THREE                       BSP_LED_2_MASK
#define LED_FOUR                        BSP_LED_3_MASK
#define ALL_APP_LED                    (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                        BSP_LED_2_MASK | BSP_LED_3_MASK)

#define BUTTON_ONE                      BSP_BUTTON_0
#define BUTTON_TWO                      BSP_BUTTON_1
#ifdef COMMISSIONING_ENABLED
#define ERASE_BUTTON_PIN_NO             BSP_BUTTON_3                                          /**< Button used to erase commissioning settings. */
#endif // COMMISSIONING_ENABLED

#define STATUS_OFF                      0x30
#define STATUS_ON                       0x31

#define APP_TIMER_OP_QUEUE_SIZE         4                                                     /**< Size of timer operation queues. */

#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)

#define LWIP_SYS_TICK_MS                100                                                   /**< Interval for timer used as trigger to send. */
#define LED_BLINK_INTERVAL_MS           300                                                   /**< LED blinking interval. */
#define COAP_TICK_INTERVAL_MS           1000                                                  /**< Interval between periodic callbacks to CoAP module. */

#define APP_RTR_SOLICITATION_DELAY      500                                                   /**< Time before host sends an initial solicitation in ms. */

#define DEAD_BEEF                       0xDEADBEEF                                            /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME             128                                                   /**< Max length of filename to copy for the debug error handler. */

#define APP_ENABLE_LOGS                 1                                                     /**< Disable debug trace in the application. */

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

typedef enum
{
    LEDS_INACTIVE = 0,
    LEDS_CONNECTABLE_MODE,
    LEDS_IPV6_IF_DOWN,
    LEDS_IPV6_IF_UP,
} display_state_t;

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                        /**< System Timer used to service CoAP and LWIP periodically. */
eui64_t                            eui64_local_iid;                                            /**< Local EUI64 value that is used as the IID for SLAAC. */
static ipv6_medium_instance_t      m_ipv6_medium;
static bool                        m_is_interface_up;
static display_state_t             m_display_state = LEDS_INACTIVE;                            /**< Board LED display state. */
static const char                  m_uri_part_lights[] = "lights";
static const char                  m_uri_part_led3[]   = "led3";
static uint16_t                    m_led3_observe_token;
static uint16_t                    m_global_token_count = 0x0102;

#ifdef COMMISSIONING_ENABLED
static bool                        m_power_off_on_failure = false;
static bool                        m_identity_mode_active;
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
            LEDS_OFF((LED_THREE | LED_FOUR));
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


/**@brief Function for handling the timer used to trigger TCP actions.
 *
 * @details This function is used to trigger TCP connection request and to send data on the TCP port.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void app_lwip_time_tick(iot_timer_time_in_ms_t wall_clock_value)
{
    sys_check_timeouts();
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
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create a sys timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,
                                APP_TIMER_MODE_REPEATED,
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
}


static void coap_response_handler(uint32_t status, void * p_arg, coap_message_t * p_response)
{
    if (status == NRF_SUCCESS)
    {
        if (coap_message_opt_present(p_response, COAP_OPT_OBSERVE) == NRF_SUCCESS)
        {
            // It is an notification or result of a GET response
            LEDS_ON(LED_FOUR);
        }
        else
        {
            LEDS_OFF(LED_FOUR);
        }


        APPL_LOG("[APPL]: Response Code : %d\r\n", p_response->header.code);
        if (p_response->header.code == COAP_CODE_205_CONTENT)
        {
            switch (p_response->p_payload[0])
            {
                case STATUS_ON:
                    LEDS_ON(LED_THREE);
                    break;

                case STATUS_OFF:
                    LEDS_OFF(LED_THREE);
                    break;

                default:
                    break;
            }
        }
    }
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

    if (m_is_interface_up == false)
    {
        return;
    }

    uint32_t err_code;
    if (button_action == APP_BUTTON_PUSH)
    {
        coap_message_t      * p_request = NULL;

        switch (pin_no)
        {
            case BUTTON_ONE:
            {
                coap_message_conf_t   message_conf;
                memset(&message_conf, 0x00, sizeof(message_conf));
                coap_remote_t         remote_device;
                
                message_conf.type             = COAP_MESSAGE_TYPE;
                message_conf.code             = COAP_CODE_GET;
                message_conf.port.port_number = LOCAL_PORT_NUM;
                message_conf.id               = 0;

                // Set token.
                m_led3_observe_token = m_global_token_count;
                (void)uint16_encode(HTONS(m_global_token_count), message_conf.token);
                m_global_token_count++;

                message_conf.token_len = 2;
                message_conf.response_callback = coap_response_handler;

                err_code = coap_message_new(&p_request, &message_conf);
                APP_ERROR_CHECK(err_code);

                memcpy(&remote_device.addr[0], (uint8_t []){SERVER_IPV6_ADDRESS}, IPV6_ADDR_SIZE);
                remote_device.port_number = REMOTE_PORT_NUM;
                err_code = coap_message_remote_addr_set(p_request, &remote_device);
                APP_ERROR_CHECK(err_code);

                // Subscribe to the resource.
                err_code = coap_message_opt_uint_add(p_request, COAP_OPT_OBSERVE, 0);
                APP_ERROR_CHECK(err_code);

                err_code = coap_message_opt_str_add(p_request, COAP_OPT_URI_PATH, (uint8_t *)m_uri_part_lights, strlen(m_uri_part_lights));
                APP_ERROR_CHECK(err_code);

                err_code = coap_message_opt_str_add(p_request, COAP_OPT_URI_PATH, (uint8_t *)m_uri_part_led3, strlen(m_uri_part_led3));
                APP_ERROR_CHECK(err_code);

                err_code = coap_message_opt_uint_add(p_request, COAP_OPT_ACCEPT, 0);
                APP_ERROR_CHECK(err_code);

                break;
            }
            case BUTTON_TWO:
            {
                coap_message_conf_t   message_conf;
                memset(&message_conf, 0x00, sizeof(message_conf));
                coap_remote_t         remote_device;
                
                message_conf.type             = COAP_MESSAGE_TYPE;
                message_conf.code             = COAP_CODE_GET;
                message_conf.port.port_number = LOCAL_PORT_NUM;
                message_conf.id               = 0;

                // Re-use the original token for the observe.
                (void)uint16_encode(HTONS(m_led3_observe_token), message_conf.token);

                message_conf.token_len = 2;
                message_conf.response_callback = coap_response_handler;

                err_code = coap_message_new(&p_request, &message_conf);
                APP_ERROR_CHECK(err_code);

                memcpy(&remote_device.addr[0], (uint8_t []){SERVER_IPV6_ADDRESS}, IPV6_ADDR_SIZE);
                remote_device.port_number = REMOTE_PORT_NUM;
                err_code = coap_message_remote_addr_set(p_request, &remote_device);
                APP_ERROR_CHECK(err_code);

                // Unsubscribe to the resource.
                err_code = coap_message_opt_uint_add(p_request, COAP_OPT_OBSERVE, 1);
                APP_ERROR_CHECK(err_code);

                // Copy the original options (draft-ietf-core-observe 3.6).
                err_code = coap_message_opt_str_add(p_request, COAP_OPT_URI_PATH, (uint8_t *)m_uri_part_lights, strlen(m_uri_part_lights));
                APP_ERROR_CHECK(err_code);

                err_code = coap_message_opt_str_add(p_request, COAP_OPT_URI_PATH, (uint8_t *)m_uri_part_led3, strlen(m_uri_part_led3));
                APP_ERROR_CHECK(err_code);

                err_code = coap_message_opt_uint_add(p_request, COAP_OPT_ACCEPT, 0);
                APP_ERROR_CHECK(err_code);

                break;
            }
            default:
                break;
        }

        // TODO: If there is no more room for subscribing to observable resources, we should
        //       remove the observable option before sending the message.

        if (p_request != NULL)
        {
            uint32_t msg_handle;
            err_code = coap_message_send(&msg_handle, p_request);

            if (err_code != NRF_SUCCESS)
            {
                APPL_LOG("[APPL]: CoAP Message skipped, error code = 0x%08lX.\r\n", err_code);
            }
            err_code = coap_message_delete(p_request);
            APP_ERROR_CHECK(err_code);
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
        {BUTTON_ONE,          false, BUTTON_PULL, button_event_handler},
        {BUTTON_TWO,          false, BUTTON_PULL, button_event_handler},
#ifdef COMMISSIONING_ENABLED
        {ERASE_BUTTON_PIN_NO, false, BUTTON_PULL, button_event_handler}
#endif // COMMISSIONING_ENABLED
    };

    err_code =  app_button_init(buttons, sizeof(buttons) / sizeof(buttons[0]), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function to handle interface up event. */
void nrf_driver_interface_up(void)
{
    m_is_interface_up = true;

#ifdef COMMISSIONING_ENABLED
    commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_STOP_RESET);
#endif // COMMISSIONING_ENABLED

    APPL_LOG ("[APPL]: IPv6 interface up.\r\n");

    sys_check_timeouts();

    m_display_state = LEDS_IPV6_IF_UP;
}


/**@brief Function to handle interface down event. */
void nrf_driver_interface_down(void)
{
    m_is_interface_up = false;

#ifdef COMMISSIONING_ENABLED
    commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED

    APPL_LOG ("[APPL]: IPv6 interface down.\r\n");

    m_display_state = LEDS_IPV6_IF_DOWN;
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

    //Initialize memory manager.
    err_code = nrf_mem_init();
    APP_ERROR_CHECK(err_code);

    //Initialize lwip stack driver.
    err_code = nrf_driver_init();
    APP_ERROR_CHECK(err_code);

    //Initialize lwip stack.
    lwip_init();
}


/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for catering CoAP module with periodic time ticks.
*/
static void app_coap_time_tick(iot_timer_time_in_ms_t wall_clock_value)
{
    (void)coap_time_tick();

    // Check if any of the observable has reached max-age timeout.
    coap_observable_t * p_observable = NULL;
    uint32_t handle;
    while (coap_observe_client_next_get(&p_observable, &handle, p_observable) == NRF_SUCCESS)
    {
        p_observable->max_age--;

        // Max age timeout
        if (p_observable->max_age == 0)
        {
            // unregister observable resource.
            uint32_t err_code = coap_observe_client_unregister(handle);
            APP_ERROR_CHECK(err_code);

            LEDS_OFF(LED_FOUR);
        }
    }
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {app_lwip_time_tick,      LWIP_SYS_TICK_MS},
        {blink_timeout_handler,   LED_BLINK_INTERVAL_MS},
        {app_coap_time_tick,      COAP_TICK_INTERVAL_MS},
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


static void coap_error_handler(uint32_t error_code, coap_message_t * p_message)
{
    // If any response fill the p_response with a appropiate response message.
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

    coap_port_t local_port_list[COAP_PORT_COUNT] = 
    {
        {.port_number = LOCAL_PORT_NUM}
    };
    
    coap_transport_init_t port_list;
    port_list.p_port_table = &local_port_list[0];

    err_code = coap_init(17, &port_list);
    APP_ERROR_CHECK(err_code);

    err_code = coap_error_handler_register(coap_error_handler);
    APP_ERROR_CHECK(err_code);

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
