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
 * @defgroup iot_coap_exosite_observer main.c
 * @{
 * @ingroup iot_sdk_app_ipv6
 * @brief This file contains the source code for CoAP Exosite client.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "mem_manager.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "ipv6_api.h"
#include "icmp6_api.h"
#include "udp_api.h"
#include "app_trace.h"
#include "app_timer.h"
#include "app_button.h"
#include "app_util.h"
#include "dns6_api.h"
#include "iot_timer.h"
#include "coap_api.h"
#include "coap_observe_api.h"
#include "ipv6_medium.h"

#define LED_ONE                            BSP_LED_0_MASK
#define LED_TWO                            BSP_LED_1_MASK
#define LED_THREE                          BSP_LED_2_MASK
#define LED_FOUR                           BSP_LED_3_MASK
#define ALL_APP_LED                       (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                           BSP_LED_2_MASK | BSP_LED_3_MASK)                         /**< Define used for simultaneous operation of all application LEDs. */
#define COAP_OBSERVE_TOGGLE_BUTTON         BSP_BUTTON_0                                             /**< Button used to (re-)subscribe to notifications. */
#ifdef COMMISSIONING_ENABLED
#define ERASE_BUTTON_PIN_NO                BSP_BUTTON_3                                             /**< Button used to erase commissioning settings. */
#endif // COMMISSIONING_ENABLED

#define LOCAL_DNS_UDP_PORT                 0x1717                                                   /**< UDP port for DNS client module. */
#define SERVER_DNS_UDP_PORT                0x0035                                                   /**< UDP Port number of DNS Server. */
#define APP_DNS_SERVER_ADDR                {{0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0x00, 0x00,  \
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88}}       /**< IPv6 address of DNS Server. */

// The CoAP default port number 5683 MUST be supported by a server.
#define LOCAL_PORT_NUM                     5683                                                     /**< CoAP default port number. */
#define REMOTE_PORT_NUM                    5683                                                     /**< Remote port number. */

#define APP_TIMER_OP_QUEUE_SIZE            5                                                        /**< Size of timer operation queues. */

#define LED_BLINK_INTERVAL_MS              300                                                      /**< LED blinking interval. */
#define COAP_TICK_INTERVAL_MS              1000                                                     /**< Interval between ticks provided for the CoAP module. */ 

#define DEAD_BEEF                          0xDEADBEEF                                               /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME                128                                                      /**< Max length of filename to copy for the debug error handler. */

#define APP_ENABLE_LOGS                    1                                                        /**< Disable debug trace in the application. */

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
    APP_STATE_INACTIVE = 0,
    APP_STATE_CONNECTABLE_MODE,
    APP_STATE_IPV6_IF_DOWN,
    APP_STATE_IPV6_IF_UP,
    APP_STATE_DNS_QUERY_IN_PROGRESS,
    APP_STATE_COAP_REQUEST_IN_PROGRESS
} application_state_t;

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                             /**< App timer instance used to update the IoT timer wall clock. */
static ipv6_medium_instance_t m_ipv6_medium;
eui64_t                       eui64_local_iid;                                                      /**< Local EUI64 value that is used as the IID for*/
static bool                   m_is_ipv6_if_up = false;                                              /**< Variable representing the state of the IPv6 interface. */
static bool                   m_observing_dataport = false;                                         /**< Variable representing whether subscribed to a dataport. */
static bool                   m_button_pressed = false;                                             /**< Variable representing whether a respone to a subscription request is expected.  */
static application_state_t    m_app_state = APP_STATE_INACTIVE;                                     /**< Application state, displayed also by the LEDs on the board. */
static const char             m_coap_server_hostname[] = "coap.exosite.com";                        /**< Host name to resolve and target for CoAP request. */
static ipv6_addr_t            m_coap_server_address;                                                /**< IPv6 address of the CoAP server. */
static const char             m_uri_part_1a[]    = "1a";                                            /**< Value of a CoAP Uri-Path option. See Exosite CoAP API doc for details. */
static const char             m_uri_part_alias[] = "nordicsrc01";                                   /**< Value of a CoAP Uri-Path option. Dataport alias. See Exosite CoAP API doc for details. */
static const char             m_uri_query_CIK[]  = "add your CIK here";                             /**< Value of a CoAP Uri-Query option. Client Interface Key. See Exosite CoAP API doc for details. */
static uint16_t               m_global_token_count = 0x0103;                                        /**< Token value of CoAP messages. */

#ifdef COMMISSIONING_ENABLED
static bool                    m_power_off_on_failure = false;
static bool                    m_identity_mode_active;
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
    switch (m_app_state)
    {
        case APP_STATE_INACTIVE:
        {
            LEDS_OFF(LED_ONE | LED_TWO | LED_THREE | LED_FOUR);
            break;
        }
        case APP_STATE_CONNECTABLE_MODE:
        {
            LEDS_INVERT(LED_ONE);
            LEDS_OFF(LED_TWO | LED_THREE | LED_FOUR);
            break;
        }
        case APP_STATE_IPV6_IF_DOWN:
        {
            LEDS_ON(LED_ONE);
            LEDS_INVERT(LED_TWO);
            LEDS_OFF(LED_THREE | LED_FOUR);
            break;
        }
        case APP_STATE_IPV6_IF_UP:
        {
            LEDS_OFF(LED_ONE | LED_FOUR);
            LEDS_ON(LED_TWO);
            if (m_observing_dataport == false)
            {
                LEDS_OFF(LED_THREE);
            }
            else
            {
                LEDS_ON(LED_THREE);
            }           
            break;
        }
        case APP_STATE_DNS_QUERY_IN_PROGRESS:
        {
            LEDS_OFF(LED_ONE | LED_FOUR);
            LEDS_ON(LED_TWO);
            LEDS_INVERT(LED_THREE);
            break;
        }
        case APP_STATE_COAP_REQUEST_IN_PROGRESS:
        {
            LEDS_OFF(LED_ONE);
            LEDS_ON(LED_TWO);
            LEDS_INVERT(LED_FOUR);
            if (m_observing_dataport == false)
            {
                LEDS_OFF(LED_THREE);
            }
            else
            {
                LEDS_ON(LED_THREE);
            }

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


static void coap_response_handler(uint32_t status, void * arg, coap_message_t * p_response)
{
    if (status == NRF_SUCCESS)
    {
        if (m_button_pressed == true)
        {
            if (coap_message_opt_present(p_response, COAP_OPT_OBSERVE) == NRF_SUCCESS)
            {
                APPL_LOG("[APPL]: Response to subscription request received.\r\n");
            }
            APPL_LOG("[APPL]: Response Code : %d\r\n\r\n", p_response->header.code);
        }
        m_button_pressed = false;

        if (p_response->header.code == COAP_CODE_205_CONTENT)
        {
            LEDS_OFF(LED_THREE | LED_FOUR);

            char rx_data[5];
            memset(rx_data, '\0', sizeof(rx_data));
            memcpy(rx_data, p_response->p_payload, p_response->payload_len);
            int payload_int = atoi(rx_data);
            if ((payload_int < -20) || (payload_int > 20))
            {
                APPL_LOG("[APPL]: Notification received. Dataport value: %5d\r\n", payload_int);
            }
            else
            {
                payload_int += 30;
                APPL_LOG("[APPL]:    ");
                uint8_t temp;
                for (temp = 0; temp < payload_int; temp++)
                {
                    APPL_LOG(" ");
                }
                APPL_LOG("*\r\n");
            }
        }
    }
    else
    {
        if (m_button_pressed == true)
        {
            APPL_LOG("[APPL]: Subscription failed.\r\n");
        }
        m_button_pressed = false;
        
        APPL_LOG("[APPL]: CoAP request not successful. \r\n\r\n");

        // Maybe server changed address; force DNS resolution.
        memset(m_coap_server_address.u8, 0x00, IPV6_ADDR_SIZE);
    }

    m_app_state = APP_STATE_IPV6_IF_UP;
}


static void coap_request_send(void)
{
    if (m_is_ipv6_if_up == false)
    {
        APPL_LOG("[APPL]: The IPv6 interface is down. \r\n\r\n");
        return;
    }

    coap_message_t      * p_request;
    coap_message_conf_t   message_conf;
    memset(&message_conf, 0x00, sizeof(message_conf));
    coap_remote_t         remote_device;

    message_conf.type = COAP_TYPE_CON;
    message_conf.code = COAP_CODE_GET;
    
    (void)uint16_encode(HTONS(m_global_token_count), message_conf.token);
    m_global_token_count++;

    message_conf.port.port_number  = LOCAL_PORT_NUM;
    message_conf.id                = 0; // Auto-generate message ID.
    message_conf.token_len         = 2;
    message_conf.response_callback = coap_response_handler;

    uint32_t err_code = coap_message_new(&p_request, &message_conf);
    APP_ERROR_CHECK(err_code);

    memcpy(&remote_device.addr[0], &m_coap_server_address.u8[0], IPV6_ADDR_SIZE);
    remote_device.port_number = REMOTE_PORT_NUM;
    err_code = coap_message_remote_addr_set(p_request, &remote_device);
    APP_ERROR_CHECK(err_code);

    // Subscribe to the resource.
    err_code = coap_message_opt_uint_add(p_request, COAP_OPT_OBSERVE, 0);
    APP_ERROR_CHECK(err_code);

    err_code = coap_message_opt_str_add(p_request,                   \
                                        COAP_OPT_URI_PATH,           \
                                        (uint8_t *)m_uri_part_1a,    \
                                        strlen(m_uri_part_1a));
    APP_ERROR_CHECK(err_code);

    err_code = coap_message_opt_str_add(p_request,                   \
                                        COAP_OPT_URI_PATH,           \
                                        (uint8_t *)m_uri_part_alias, \
                                        strlen(m_uri_part_alias));
    APP_ERROR_CHECK(err_code);

    err_code = coap_message_opt_str_add(p_request,                   \
                                        COAP_OPT_URI_QUERY,          \
                                        (uint8_t *)m_uri_query_CIK,  \
                                        strlen(m_uri_query_CIK));
    APP_ERROR_CHECK(err_code);

    uint32_t handle;
    err_code = coap_message_send(&handle, p_request);
    if (err_code != NRF_SUCCESS)
    {
        APPL_LOG("[APPL]: CoAP Message skipped, error code = 0x%08lX.\r\n", err_code);
    }
    else
    {
        m_app_state = APP_STATE_COAP_REQUEST_IN_PROGRESS;
    }

    err_code = coap_message_delete(p_request);
    APP_ERROR_CHECK(err_code);
}


/**@brief DNS6 module event handler.
 *
 * @details Callback registered with the DNS6 module to receive asynchronous events from
 *          the module for registering query.
 */
static void app_dns_handler(uint32_t      process_result,
                            const char  * p_hostname,
                            ipv6_addr_t * p_addr,
                            uint16_t      addr_count)
{
    uint32_t index;

    if (m_app_state != APP_STATE_DNS_QUERY_IN_PROGRESS)
    {
        // Exit if not in resolving state.
        return;
    }

    APPL_LOG("[APPL]: DNS Response for hostname: %s, with %d IPv6 addresses " \
             "and status 0x%08lX\r\n", p_hostname, addr_count, process_result);

    if (process_result == NRF_SUCCESS)
    {
        for (index = 0; index < addr_count; index++)
        {
            APPL_LOG("[APPL]: [%ld] IPv6 Address: ", index);
            APPL_ADDR(p_addr[index]);

            // Store only first given address, but print all of them.
            if(index == 0)
            {
                memcpy(m_coap_server_address.u8, p_addr[0].u8, IPV6_ADDR_SIZE);
            }
        }
        // Change application state.
        m_app_state = APP_STATE_IPV6_IF_UP;

        // Send a CoAP request to the resolved address.
        coap_request_send();
    }
    else
    {
        APPL_LOG("[APPL]: DNS query failed.\r\n");
        m_app_state = APP_STATE_IPV6_IF_UP;
    }
}


static void coap_request_commence(void)
{
    uint32_t err_code = NRF_SUCCESS;
    if ((m_app_state == APP_STATE_DNS_QUERY_IN_PROGRESS)     ||  \
        (m_app_state == APP_STATE_COAP_REQUEST_IN_PROGRESS))
    {
        // The device is busy.
        if (m_app_state == APP_STATE_DNS_QUERY_IN_PROGRESS)
        {
            APPL_LOG("[APPL]: Busy: DNS query in progress.\r\n");
        }
        else if (m_app_state == APP_STATE_COAP_REQUEST_IN_PROGRESS)
        {
            APPL_LOG("[APPL]: Busy: CoAP request in progress.\r\n");
        }

        return;
    }
    else
    {
        // Check if the hostname was resolved.
        if (0 == memcmp(m_coap_server_address.u8,                                     \
                        (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  \
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, \
                        IPV6_ADDR_SIZE))
        {
            APPL_LOG("[APPL]: CoAP server IPv6 address not available, sending DNS query... \r\n");

            err_code = dns6_query(m_coap_server_hostname, app_dns_handler);
            APP_ERROR_CHECK(err_code);
            
            m_app_state = APP_STATE_DNS_QUERY_IN_PROGRESS;
        }
        else
        {
            APPL_LOG("[APPL]: Sending CoAP request... \r\n");

            coap_request_send();
        }

        return;
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

    // Check if IPv6 interface is UP.
    if (m_is_ipv6_if_up == false)
    {
        APPL_LOG("[APPL]: The IPv6 interface is down. \r\n\r\n");
        return;
    }

    if (button_action == APP_BUTTON_PUSH)
    {
        if (pin_no == COAP_OBSERVE_TOGGLE_BUTTON)
        {
            if (m_observing_dataport == true)
            {
                APPL_LOG("\r\n[APPL]: Re-subscribing for CoAP resource " \
                         "state change notifications. \r\n\r\n");
            }
            else if (m_observing_dataport == false)
            {
                m_observing_dataport = true;
                APPL_LOG("\r\n[APPL]: Subscribing for CoAP resource "    \
                         "state change notifications. \r\n\r\n");
            }

            m_button_pressed = true;
            coap_request_commence();
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
        {COAP_OBSERVE_TOGGLE_BUTTON, false, BUTTON_PULL, button_event_handler},
#ifdef COMMISSIONING_ENABLED
        {ERASE_BUTTON_PIN_NO,        false, BUTTON_PULL, button_event_handler}
#endif // COMMISSIONING_ENABLED
    };

    #define BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)

    err_code = app_button_init(buttons, \
                               sizeof(buttons) / sizeof(buttons[0]), \
                               BUTTON_DETECTION_DELAY);
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
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    uint32_t err_code;
    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);

    // Initialize timer instance as a tick source for IoT timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,   \
                                APP_TIMER_MODE_REPEATED,    \
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
}


/**@brief ICMP6 module event handler.
 *
 * @details Callback registered with the ICMP6 module to receive asynchronous events from 
 *          the module, if the ICMP6_ENABLE_ALL_MESSAGES_TO_APPLICATION constant is not zero 
 *          or the ICMP6_ENABLE_ND6_MESSAGES_TO_APPLICATION constant is not zero.
 */
uint32_t icmp6_handler(iot_interface_t  * p_interface,
                       ipv6_header_t    * p_ip_header,
                       icmp6_header_t   * p_icmp_header,
                       uint32_t           process_result,
                       iot_pbuffer_t    * p_rx_packet)
{
    #define ICMP_LOG(...)
    #define ICMP_ADDR(...)
    ICMP_LOG("[APPL]: Got ICMP6 Application Handler Event on interface 0x%p\r\n", p_interface);

    ICMP_LOG("[APPL]: Source IPv6 Address: ");
    ICMP_ADDR(p_ip_header->srcaddr);
    ICMP_LOG("[APPL]: Destination IPv6 Address: ");
    ICMP_ADDR(p_ip_header->destaddr);
    ICMP_LOG("[APPL]: Process result = 0x%08lx\r\n", process_result);

    switch(p_icmp_header->type)
    {
        case ICMP6_TYPE_DESTINATION_UNREACHABLE:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Destination Unreachable Error\r\n");
            break;
        case ICMP6_TYPE_PACKET_TOO_LONG:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Packet Too Long Error\r\n");
            break;
        case ICMP6_TYPE_TIME_EXCEED:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Time Exceed Error\r\n");
            break;
        case ICMP6_TYPE_PARAMETER_PROBLEM:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Parameter Problem Error\r\n");
            break;
        case ICMP6_TYPE_ECHO_REQUEST:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Echo Request\r\n");
            break;
        case ICMP6_TYPE_ECHO_REPLY:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Echo Reply\r\n");
            break;
        case ICMP6_TYPE_ROUTER_SOLICITATION:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Router Solicitation\r\n");
            break;
        case ICMP6_TYPE_ROUTER_ADVERTISEMENT:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Router Advertisement\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_SOLICITATION:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Neighbor Solicitation\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_ADVERTISEMENT:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Neighbor Advertisement\r\n");
            break;
        default:
            break;
    }

    return NRF_SUCCESS;
}


/**@brief IP stack event handler.
 *
 * @details Callback registered with the ICMP6 to receive asynchronous events from the module.
 */
void ip_app_handler(iot_interface_t * p_interface, ipv6_event_t * p_event)
{
    APPL_LOG("[APPL]: Got IP Application Handler Event on interface 0x%p\r\n", p_interface);

    switch(p_event->event_id)
    {
        case IPV6_EVT_INTERFACE_ADD:
#ifdef COMMISSIONING_ENABLED
            commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_STOP_RESET);
#endif // COMMISSIONING_ENABLED
            APPL_LOG("[APPL]: New interface added!\r\n");

            memset(m_coap_server_address.u8, 0x00, IPV6_ADDR_SIZE);
            m_is_ipv6_if_up = true;
            m_app_state     = APP_STATE_IPV6_IF_UP;
            break;
        case IPV6_EVT_INTERFACE_DELETE:
#ifdef COMMISSIONING_ENABLED
            commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED
            APPL_LOG("[APPL]: Interface removed!\r\n");

            m_is_ipv6_if_up      = false;
            m_observing_dataport = false;
            m_app_state          = APP_STATE_IPV6_IF_DOWN;
            break;
        default:
            //Unknown event. Should not happen.
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

    err_code = icmp6_receive_register(icmp6_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing IP stack.
 *
 * @details Initialize the IP Stack.
 */
static void dns_client_init(void)
{
    uint32_t    err_code;

    // Configure DNS client and server parameters.
    dns6_init_t dns_init_param =
    {
        .local_src_port  = LOCAL_DNS_UDP_PORT,
        .dns_server      =
        {
            .port = SERVER_DNS_UDP_PORT,
            .addr = APP_DNS_SERVER_ADDR
        }
    };

    err_code = dns6_init(&dns_init_param);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling CoAP periodically time ticks.
*/
static void app_coap_time_tick(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);

    // Pass a tick to coap in order to re-transmit any pending messages.
    (void)coap_time_tick();
    
    // Check if any of the observable has reached max-age timeout.
    coap_observable_t * p_observable = NULL;
    uint32_t handle;
    while (coap_observe_client_next_get(&p_observable, &handle, p_observable) == NRF_SUCCESS)
    {
        p_observable->max_age--;

        // Max age timeout.
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
        {blink_timeout_handler,   LED_BLINK_INTERVAL_MS},
        {app_coap_time_tick,      COAP_TICK_INTERVAL_MS},
        {dns6_timeout_process,    SEC_TO_MILLISEC(DNS6_RETRANSMISSION_INTERVAL)},
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
    APPL_LOG("[APPL]: CoAP error.\r\n");
    // If any response fill the p_response with a appropiate response message. 
}


/**@brief Function for starting connectable mode.
 */
static void connectable_mode_enter(void)
{
    uint32_t err_code = ipv6_medium_connectable_mode_enter(m_ipv6_medium.ipv6_medium_instance_id);
    APP_ERROR_CHECK(err_code);

    APPL_LOG("[APPL]: Physical layer in connectable mode.\r\n");
    m_app_state = APP_STATE_CONNECTABLE_MODE;
}


static void on_ipv6_medium_evt(ipv6_medium_evt_t * p_ipv6_medium_evt)
{
    switch (p_ipv6_medium_evt->ipv6_medium_evt_id)
    {
        case IPV6_MEDIUM_EVT_CONN_UP:
        {
            APPL_LOG("[APPL]: Physical layer: connected.\r\n");
            m_app_state = APP_STATE_IPV6_IF_DOWN;
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

    // Initialize.
    app_trace_init();
    leds_init();

#ifdef COMMISSIONING_ENABLED
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);
#endif // COMMISSIONING_ENABLED

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

    ip_stack_init();
    dns_client_init();

    // Initialize IoT timer module.
    iot_timer_init();

    uint8_t rnd_seed;
    do
    {
        err_code = sd_rand_application_vector_get(&rnd_seed, 1);
    } while (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES);

    m_global_token_count += rnd_seed;

    coap_port_t local_port_list[COAP_PORT_COUNT] = 
    {
        {.port_number = LOCAL_PORT_NUM}
    };
    
    coap_transport_init_t port_list;
    port_list.p_port_table = &local_port_list[0];

    err_code = coap_init(rnd_seed, &port_list);
    APP_ERROR_CHECK(err_code);

    err_code = coap_error_handler_register(coap_error_handler);
    APP_ERROR_CHECK(err_code);

    APPL_LOG("\r\n\r\n");
    APPL_LOG("[APPL]: Init complete.\r\n");

    // Start execution.
    connectable_mode_enter();

    // Enter main loop.
    for (;;)
    {
        /* Sleep while waiting for an application event. */
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

/**
 * @}
 */
