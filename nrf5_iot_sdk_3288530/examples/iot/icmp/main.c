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
 * @defgroup iot_sdk_icmp_app main.c
 * @{
 * @ingroup iot_sdk_app_ipv6
 * @brief This file contains the source code for Nordic's IPv6 ICMPv6 sample application.
 *
 * This example demonstrates how Nordic's IPv6 stack can be used for sending and receiving ICMPv6 packets.
 * Sending one of four packets is triggered by user buttons on kit. LEDS blinking indicates that response has been received.
 *
 * All available packets are:
 * - Neighbor Solicitation                              | Button 1
 * - Router Solicitation                                | Button 2
 * - Echo Request to all node multicast address ff02::1 | Button 3
 * - Echo Request to remote node (Router)               | Button 4
 *
 * Note: In order to get response for Router Solicitation, peer node has to act as a Router.
 *
 *  Below is MSC explaining the data flow at ICMPv6 level.
 *
 *    +------------------+                                                +------------------+
 *    | Node             |                                                | Router           |
 *    |(this application)|                                                |                  |
 *    +------------------+                                                +------------------+
 *           |                                                                   |
 *           |                                                                   |
 *        ---|------------------------- Interface is UP  ------------------------|---
 *           |                                                                   |
 *        ---|-------------------- Button 1 has been pushed ---------------------|---
 *           |                                                                   |
 *           |[ICMPv6 Neighbor Solicitation to router's link-local address]      |
 *           |------------------------------------------------------------------>|
 *           |                                                                   |
 *           |       [ICMPv6 Neighbor Advertisement to node's link-local address]|
 *           |<------------------------------------------------------------------|
 *           |                                                                   |
 *     LEDS BLINKING                                                             |
 *           |                                                                   |
 *        ---|-------------------- Button 2 has been pushed ---------------------|---
 *           |                                                                   |
 *           |[ICMPv6 Router Solicitation to all routers multicast address]      |
 *           |------------------------------------------------------------------>|
 *           |                                                                   |
 *           |         [ICMPv6 Router Advertisement to node's link-local address]|
 *           |<------------------------------------------------------------------|
 *           |                                                                   |
 *     LEDS BLINKING                                                             |
 *           |                                                                   |
 *        ---|-------------------- Button 3 has been pushed ---------------------|---
 *           |                                                                   |
 *           |[ICMPv6 Echo Request to all nodes multicast address]               |
 *           |------------------------------------------------------------------>|
 *           |                                                                   |
 *           |                [ICMPv6 Echo Response to node's link-local address]|
 *           |<------------------------------------------------------------------|
 *           |                                                                   |
 *     LEDS BLINKING                                                             |
 *           |                                                                   |
 *        ---|-------------------- Button 4 has been pushed ---------------------|---
 *           |                                                                   |
 *           |[ICMPv6 Echo Request to router's link-local address]               |
 *           |------------------------------------------------------------------>|
 *           |                                                                   |
 *           |                [ICMPv6 Echo Response to node's link-local address]|
 *           |<------------------------------------------------------------------|
 *           |                                                                   |
 *     LEDS BLINKING                                                             |
 *           |                                                                   |
 *           -                                                                   -
 */


#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "nordic_common.h"
#include "softdevice_handler_appsh.h"
#include "app_scheduler.h"
#include "mem_manager.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "app_trace.h"
#include "app_timer_appsh.h"
#include "app_button.h"
#include "ble_ipsp.h"
#include "ipv6_api.h"
#include "icmp6_api.h"
#include "udp_api.h"
#include "iot_timer.h"
#include "ipv6_medium.h"

#define CONNECTABLE_MODE_LED          BSP_LED_0_MASK                                                /**< Led to indicate that device is in connectable mode. */
#define CONNECTED_LED                 BSP_LED_1_MASK                                                /**< Led to indicate that device is connected. */
#define RESPONSE_LED_1                BSP_LED_2_MASK                                                /**< Led to indicate ICMP response is received. */
#define RESPONSE_LED_2                BSP_LED_3_MASK                                                /**< Led to indicate ICMP response is received. */

#define NS_BUTTON_PIN_NO              BSP_BUTTON_0                                                  /**< Button used to send Neighbor Solicitation message. */
#define RS_BUTTON_PIN_NO              BSP_BUTTON_1                                                  /**< Button used to send Router Solicitation message. */
#define PING_ALL_BUTTON_PIN_NO        BSP_BUTTON_2                                                  /**< Button used to send Echo Request to all device. */
#define PING_PEER_BUTTON_PIN_NO       BSP_BUTTON_3                                                  /**< Button used to send Echo Request to peer device. */

#define APP_TIMER_OP_QUEUE_SIZE       5                                                             /**< Size of timer operation queues. */

#define SCHED_MAX_EVENT_DATA_SIZE     128                                                           /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE              8                                                             /**< Maximum number of events in the scheduler queue. */

#define DEAD_BEEF                     0xDEADBEEF                                                    /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME           128                                                           /**< Max length of filename to copy for the debug error handler. */

#define LED_BLINK_INTERVAL_MS         400                                                           /**< Timeout between LEDS toggling. */
#define APP_MAX_LED_TOGGLE            5                                                             /**< Number of LEDS toggling after successful operation. */

#define APPL_LOG  app_trace_log
#define APPL_DUMP app_trace_dump

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
    LEDS_BLINK_MODE
} display_state_t;

eui64_t                       eui64_local_iid;                                                      /**< Local EUI64 value that is used as the IID for SLAAC. */
static iot_interface_t      * mp_interface = NULL;                                                  /**< Pointer to IoT interface if any. */
static ipv6_medium_instance_t m_ipv6_medium;
static display_state_t        m_disp_state = LEDS_INACTIVE;                                         /**< Board LED display state. */
static uint8_t                m_blink_count = 0;                                                    /**< Number of LED flashes after successful operation. */
static bool                   m_led_feedback_enabled = false;                                           /**< If true, incoming ICMP packets are notified to the user. */

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                             /**< App timer instance used to update the IoT timer wall clock. */

/**@brief IPv6 well-known addresses. */
static const ipv6_addr_t      all_node_multicast_addr   = {{0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};
static const ipv6_addr_t      all_router_multicast_addr = {{0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}};

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
    LEDS_ON(LEDS_MASK);
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
        LEDS_OFF((BSP_LED_0_MASK | BSP_LED_1_MASK | BSP_LED_2_MASK | BSP_LED_3_MASK));
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
    LEDS_CONFIGURE(LEDS_MASK);

    // Set LEDS off.
    LEDS_OFF(LEDS_MASK);
}


/**@brief Timer callback used for controlling board LEDs to represent application state.
 */
static void blink_timeout_handler(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);

    static display_state_t previous_display_state;
#ifdef COMMISSIONING_ENABLED
    static bool id_mode_previously_enabled;
#endif // COMMISSIONING_ENABLED

#ifdef COMMISSIONING_ENABLED
    if (m_identity_mode_active == false)
    {
#endif // COMMISSIONING_ENABLED
    switch (m_disp_state)
    {
        case LEDS_INACTIVE:
        {
            LEDS_OFF(LEDS_MASK);
            break;
        }
        case LEDS_CONNECTABLE_MODE:
        {
            LEDS_INVERT(CONNECTABLE_MODE_LED);
            LEDS_OFF((CONNECTED_LED | RESPONSE_LED_1 | RESPONSE_LED_2));
            break;
        }
        case LEDS_IPV6_IF_DOWN:
        {
            LEDS_ON(CONNECTABLE_MODE_LED);
            LEDS_OFF((CONNECTED_LED | RESPONSE_LED_1 | RESPONSE_LED_2));
            break;
        }
        case LEDS_IPV6_IF_UP:
        {
            LEDS_ON(CONNECTED_LED);
            LEDS_OFF((CONNECTABLE_MODE_LED | RESPONSE_LED_1 | RESPONSE_LED_2));
            break;
        }
        case LEDS_BLINK_MODE:
        {          
            if (previous_display_state != LEDS_BLINK_MODE)
            {
                LEDS_OFF(RESPONSE_LED_1 | RESPONSE_LED_2);
            }
            else
            {
                ++m_blink_count;
                if (m_blink_count < APP_MAX_LED_TOGGLE)
                {
                    LEDS_INVERT(RESPONSE_LED_1 | RESPONSE_LED_2);
                }
                else
                {
                    LEDS_OFF(RESPONSE_LED_1 | RESPONSE_LED_2);
                    m_disp_state = LEDS_IPV6_IF_UP;
                }
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
    previous_display_state = m_disp_state;

#ifdef COMMISSIONING_ENABLED
    if ((id_mode_previously_enabled == false) && (m_identity_mode_active == true))
    {
        LEDS_ON(BSP_LED_0_MASK | BSP_LED_2_MASK);
        LEDS_OFF(BSP_LED_1_MASK | BSP_LED_3_MASK);
    }

    if ((id_mode_previously_enabled == true) && (m_identity_mode_active == true))
    {
        LEDS_INVERT(LEDS_MASK);
    }

    if ((id_mode_previously_enabled == true) && (m_identity_mode_active == false))
    {
        LEDS_OFF(LEDS_MASK);
    }

    id_mode_previously_enabled = m_identity_mode_active;
#endif // COMMISSIONING_ENABLED
}


/**@brief Function for indicating receive of ICMP response by blinkink the leds.
 */
static void led_blinking_start()
{
    m_blink_count = 0;
    m_disp_state  = LEDS_BLINK_MODE;
    m_led_feedback_enabled = false;
}


/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}


/**@brief Function for handling button events.
 *
 * @param[in]   pin_no         The pin number of the button pressed.
 * @param[in]   button_action  The action performed on button.
 */
static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
    uint32_t                   err_code;
    ipv6_addr_t                src_addr;
    ipv6_addr_t                dest_addr;
    icmp6_ns_param_t           ns_param;
    iot_pbuffer_t            * p_buffer;
    iot_pbuffer_alloc_param_t  pbuff_param;

    // Check if interface is UP.
    if (mp_interface == NULL)
    {
#ifdef COMMISSIONING_ENABLED
        if ((button_action == APP_BUTTON_PUSH) && (pin_no == PING_PEER_BUTTON_PIN_NO))
        {
            APPL_LOG("[APPL]: Erasing all commissioning settings from persistent storage... \r\n");
            commissioning_settings_clear();
            return;
        }
#endif // COMMISSIONING_ENABLED
        return;
    }

    // Create Link Local address.
    IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&src_addr, mp_interface->local_addr.identifier);

    if (button_action == APP_BUTTON_PUSH)
    {
        m_led_feedback_enabled = true;

        switch (pin_no)
        {
            case NS_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: Sending Neighbour Solicitation to peer!\r\n");

                ns_param.add_aro      = true;
                ns_param.aro_lifetime = 1000;

                // Unicast address.
                IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&ns_param.target_addr,
                                                  mp_interface->peer_addr.identifier);

                // Send Neighbour Solicitation to all nodes.
                err_code = icmp6_ns_send(mp_interface,
                                         &src_addr,
                                         &ns_param.target_addr,
                                         &ns_param);
                APP_ERROR_CHECK(err_code);
                break;
            }
            case RS_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: Sending Router Solicitation to all routers FF02::2!\r\n");

                // Send Router Solicitation to all routers.
                err_code = icmp6_rs_send(mp_interface,
                                         &src_addr,
                                         &all_router_multicast_addr);
                APP_ERROR_CHECK(err_code);
                break;
            }
            case PING_ALL_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: Ping all remote nodes using FF02::1 address!\r\n");

                pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
                pbuff_param.type   = ICMP6_PACKET_TYPE;
                pbuff_param.length = ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET + 10;

                // Allocate packet buffer.
                err_code = iot_pbuffer_allocate(&pbuff_param, &p_buffer);
                APP_ERROR_CHECK(err_code);

                // Fill payload of Echo Request with 'A' letters.
                memset(p_buffer->p_payload + ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET, 'A', 10);

                // Send Echo Request to all nodes.
                err_code = icmp6_echo_request(mp_interface, &src_addr, &all_node_multicast_addr, p_buffer);
                APP_ERROR_CHECK(err_code);
                break;
            }
            case PING_PEER_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: Ping peer device!\r\n");

                pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
                pbuff_param.type   = ICMP6_PACKET_TYPE;
                pbuff_param.length = ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET + 10;

                // Allocate packet buffer.
                err_code = iot_pbuffer_allocate(&pbuff_param, &p_buffer);
                APP_ERROR_CHECK(err_code);

                // Unicast address.
                IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&dest_addr,
                                                  mp_interface->peer_addr.identifier);

                // Fill payload of Echo Request with 'B' letters.
                memset(p_buffer->p_payload + ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET, 'A', 10);

                // Send Echo Request to all nodes.
                err_code = icmp6_echo_request(mp_interface, &src_addr, &dest_addr, p_buffer);
                APP_ERROR_CHECK(err_code);
                break;
            }

            default:
            {
                m_led_feedback_enabled = false;
                break;
            }
        }
    }
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

    // Initialize timer module
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, true);

    // Initialize timer instance as a tick source for IoT timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,   \
                                APP_TIMER_MODE_REPEATED,    \
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {blink_timeout_handler,   LED_BLINK_INTERVAL_MS},
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


/**@brief Function for the Button initialization.
 *
 * @details Initializes all Buttons used by this application.
 */
static void buttons_init(void)
{
    uint32_t err_code;

    static app_button_cfg_t buttons[] =
    {
        {NS_BUTTON_PIN_NO,          false, BUTTON_PULL, button_event_handler},
        {RS_BUTTON_PIN_NO,          false, BUTTON_PULL, button_event_handler},
        {PING_ALL_BUTTON_PIN_NO,    false, BUTTON_PULL, button_event_handler},
        {PING_PEER_BUTTON_PIN_NO,   false, BUTTON_PULL, button_event_handler}
    };

    #define BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)

    err_code = app_button_init(buttons, sizeof(buttons) / sizeof(buttons[0]), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
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
    APPL_LOG("[APPL]: Got ICMP6 Application Handler Event on interface 0x%p\r\n", p_interface);

    APP_ERROR_CHECK(process_result);

    APPL_LOG("[APPL]: Source IPv6 Address: ");
    APPL_ADDR(p_ip_header->srcaddr);
    APPL_LOG("[APPL]: Destination IPv6 Address: ");
    APPL_ADDR(p_ip_header->destaddr);
    APPL_LOG("[APPL]: Process result = 0x%08lX\r\n", process_result);

    switch(p_icmp_header->type)
    {
        case ICMP6_TYPE_DESTINATION_UNREACHABLE:
            APPL_LOG("[APPL]: ICMP6 Message Type = Destination Unreachable Error\r\n");
            break;
        case ICMP6_TYPE_PACKET_TOO_LONG:
            APPL_LOG("[APPL]: ICMP6 Message Type = Packet Too Long Error\r\n");
            break;
        case ICMP6_TYPE_TIME_EXCEED:
            APPL_LOG("[APPL]: ICMP6 Message Type = Time Exceed Error\r\n");
            break;
        case ICMP6_TYPE_PARAMETER_PROBLEM:
            APPL_LOG("[APPL]: ICMP6 Message Type = Parameter Problem Error\r\n");
            break;
        case ICMP6_TYPE_ECHO_REQUEST:
            APPL_LOG("[APPL]: ICMP6 Message Type = Echo Request\r\n");
            break;
        case ICMP6_TYPE_ECHO_REPLY:
            APPL_LOG("[APPL]: ICMP6 Message Type = Echo Reply\r\n");

            if (m_led_feedback_enabled == true)
            {
                // Start blinking LEDS to indicate that Echo Reply has been received.
                led_blinking_start();
            }

            break;
        case ICMP6_TYPE_ROUTER_SOLICITATION:
            APPL_LOG("[APPL]: ICMP6 Message Type = Router Solicitation\r\n");
            break;
        case ICMP6_TYPE_ROUTER_ADVERTISEMENT:
            APPL_LOG("[APPL]: ICMP6 Message Type = Router Advertisement\r\n");

            if (m_led_feedback_enabled == true)
            {
                // Start blinking LEDS to indicate that Echo Reply has been received.
                led_blinking_start();
            }

            break;
        case ICMP6_TYPE_NEIGHBOR_SOLICITATION:
            APPL_LOG("[APPL]: ICMP6 Message Type = Neighbor Solicitation\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_ADVERTISEMENT:
            APPL_LOG("[APPL]: ICMP6 Message Type = Neighbor Advertisement\r\n");

            if (m_led_feedback_enabled == true)
            {
                // Start blinking LEDS to indicate that Echo Reply has been received.
                led_blinking_start();
            }

            break;
        default:
            break;
    }

    return NRF_SUCCESS;
}


/**@brief Function for starting connectable mode.
 */
static void connectable_mode_enter(void)
{
    uint32_t err_code = ipv6_medium_connectable_mode_enter(m_ipv6_medium.ipv6_medium_instance_id);
    APP_ERROR_CHECK(err_code);

    APPL_LOG("[APPL]: Physical layer in connectable mode.\r\n");
    m_disp_state = LEDS_CONNECTABLE_MODE;
}


/**@brief IP Stack interface events handler. */
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
            mp_interface = p_interface;

            m_disp_state = LEDS_IPV6_IF_UP;

            break;
        case IPV6_EVT_INTERFACE_DELETE:
#ifdef COMMISSIONING_ENABLED
            commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED
            APPL_LOG("[APPL]: Interface removed!\r\n");
            mp_interface = NULL;

            m_led_feedback_enabled = false;
            m_disp_state = LEDS_IPV6_IF_DOWN;

            break;
        case IPV6_EVT_INTERFACE_RX_DATA:
            APPL_LOG("[APPL]: Got unsupported protocol data!\r\n");
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


static void on_ipv6_medium_evt(ipv6_medium_evt_t * p_ipv6_medium_evt)
{
    switch (p_ipv6_medium_evt->ipv6_medium_evt_id)
    {
        case IPV6_MEDIUM_EVT_CONN_UP:
        {
            APPL_LOG("[APPL]: Physical layer: connected.\r\n");
            m_disp_state = LEDS_IPV6_IF_DOWN;
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
            LEDS_OFF(LEDS_MASK);
            m_identity_mode_active = true;

            break;
        }
        case CMD_IDENTITY_MODE_EXIT:
        {
            m_identity_mode_active = false;
            LEDS_OFF(LEDS_MASK);

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
    scheduler_init();
    iot_timer_init();

    static ipv6_medium_init_params_t ipv6_medium_init_params;
    memset(&ipv6_medium_init_params, 0x00, sizeof(ipv6_medium_init_params));
    ipv6_medium_init_params.ipv6_medium_evt_handler    = on_ipv6_medium_evt;
    ipv6_medium_init_params.ipv6_medium_error_handler  = on_ipv6_medium_error;
    ipv6_medium_init_params.use_scheduler              = true;
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

    // Initialize IP Stack.
    ip_stack_init();

    APPL_LOG("[APPL]: Init done.\r\n");
    // Start execution.
    connectable_mode_enter();

    // Enter main loop.
    for (;;)
    {
        /* Execute event schedule */
        app_sched_execute();

        /* Sleep waiting for an application event. */
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

/**
 * @}
 */
