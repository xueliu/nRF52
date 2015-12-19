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
 * @defgroup iot_sdk_app_nrf_udp_server main.c
 * @{
 * @ingroup iot_sdk_app_nrf
 * @brief This file contains the source code for nRF UDP Server sample application.
 *
 * This application is intended to work with the Nordic UDP Client sample application.
 * Check TX_PORT and RX_PORT for port configuration.
 *
 * An Echo Response is sent in reply to an Echo Requests from any address, but
 * reception of Echo Requests from from the Client are indicated by the LEDs.
 *
 * If a UDP packet is received from the Client, it is immediately sent back regardless
 * of its contents without any modification.
 *
 * The operation of the application is reflected by two LEDs on the board:
 *
 * +-----------+-----------+
 * |   LED 1   |   LED 2   |
 * +-----------+-----------+---------------------------------------------------+
 * | Blinking  |    Off    |       Device advertising as BLE peripheral.       |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     | Blinking  |     BLE link established, IPv6 interface down.    |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     |    Off    |      BLE link established, IPv6 interface up.     |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     | Two rapid |            Echo Request received and              |
 * |           |  flashes  |              Echo Response is sent.               |
 * +-----------+-----------+---------------------------------------------------+
 * | Blinking  |    On     |   If a UDP packet is received from the Client,    |
 * |           |           | it is immediately sent back and LED 1 is toggled. |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     |    On     |       Assertion failure in the application.       |
 * +-----------+-----------+---------------------------------------------------+
 *
 * The MSC below illustrates the data flow.
 *
 *       +------------------+                                              +------------------+
 *       | UDP Client       |                                              | UDP Server       |
 *       |                  |                                              |(this application)|
 *       +------------------+                                              +------------------+
 *                |                                                                 |
 *                |                                                                 |
 *                |                                                         Listening on RX_PORT
 *                |                                                                 |
 *                |                                                                 |
 *                |[ICMPv6 Echo Request]                                            |
 *                |---------------------------------------------------------------->|
 *                |                                                                 |
 *                |                                          [ICMPv6 Echo Response] |
 *                |<----------------------------------------------------------------|
 *                |                                                                 |
 *                |[UDP Payload : [Sequence Number 1 | Data ]]                      |
 *                |---------------------------------------------------------------->|
 *                |                                                                 |
 *                |                                                    Sequence number logged on UART
 *                |                                                         if trace is enabled.
 *                |                                                                 |
 *                |                     [UDP Payload : [Sequence Number 1 | Data ]] |
 *                |<----------------------------------------------------------------|
 *                |                                                                 |
 *  Sequence number is incremented                                                  |
 *                |                                                                 |
 *                |[UDP Payload : [Sequence Number 2 | Data ]]                      |
 *                |---------------------------------------------------------------->|
 *                |                                                                 |
 *                |                                                    Sequence number logged on UART
 *                |                                                         if trace is enabled.
 *                |                                                                 |
 *                |                     [UDP Payload : [Sequence Number 2 | Data ]] |
 *                |<----------------------------------------------------------------|
 *                |                                                                 |
 *  Sequence number is incremented                                                  |
 *                |                 .                                               |
 *                |                 .                                               |
 *                |                 .                                               |
 *                |                                                                 |
 *                -                                                                 -
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
#include "app_button.h"
#include "app_trace.h"
#include "app_timer.h"
#include "iot_timer.h"
#include "ipv6_medium.h"

#define LED_ONE                       BSP_LED_0_MASK
#define LED_TWO                       BSP_LED_1_MASK
#define LED_THREE                     BSP_LED_2_MASK
#define LED_FOUR                      BSP_LED_3_MASK

#ifdef COMMISSIONING_ENABLED
#define ERASE_BUTTON_PIN_NO           BSP_BUTTON_3                                                  /**< Button used to erase commissioning settings. */
#endif // COMMISSIONING_ENABLED

#define UDP_PORT                      9000                                                          /**< Port for transmission of UDP packets. */

#define APP_TIMER_OP_QUEUE_SIZE       5                                                             /**< Size of timer operation queues. */
#define LED_BLINK_INTERVAL_MS         300                                                           /**< LED blinking interval. */
#define ECHO_REQ_BLINK_INTERVAL_MS    50                                                            /**< Echo Request indication LED blinking interval. */
#define ECHO_REQ_LED_TOGGLE_COUNT     6

#define DEAD_BEEF                     0xDEADBEEF                                                    /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME           128                                                           /**< Max length of filename to copy for the debug error handler. */

#define APP_ENABLE_LOGS               1                                                             /**< Disable debug trace in the application. */

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
    LEDS_TX_ECHO_RESPONSE,
    LEDS_TX_UDP_PACKET
} display_state_t;

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                             /**< App timer instance used to update the IoT timer wall clock. */
static ipv6_medium_instance_t m_ipv6_medium;
eui64_t                       eui64_local_iid;                                                      /**< Local EUI64 value that is used as the IID for*/
static udp6_socket_t          m_udp_socket;                                                         /**< UDP socket used for reception and transmission. */
static bool                   m_udp_tx_occured = false;                                             /**< Indicates UDP transmission for controlling board LEDs. */
static bool                   m_echo_req_received = false;                                          /**< Indicates that an Echo Request packet was received. */
static display_state_t        m_display_state = LEDS_INACTIVE;                                      /**< Board LED display state. */


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
    LEDS_ON((LED_ONE | LED_TWO));
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
            LEDS_ON(LED_ONE);
            LEDS_INVERT(LED_TWO);
            break;
        }
        case LEDS_TX_ECHO_RESPONSE:
        {
            LEDS_ON(LED_ONE);
            LEDS_OFF(LED_TWO);
            break;
        }
        case LEDS_TX_UDP_PACKET:
        {
            LEDS_ON(LED_TWO);
            if (LED_IS_ON(LED_ONE) == 0)
            {
                if (m_udp_tx_occured)
                {
                    // If UDP transmission stops, LED_ONE should stop blinking in off state.
                    LEDS_ON(LED_ONE);
                    m_udp_tx_occured = false;
                }
            }
            else
            {
                LEDS_OFF(LED_ONE);
            }

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


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 */
static void echo_req_indicate(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);

    static uint8_t toggle_counter = 0;
    if (m_echo_req_received == true)
    {
        ++toggle_counter;
        if (toggle_counter < ECHO_REQ_LED_TOGGLE_COUNT)
        {
            LEDS_INVERT(LED_TWO);
        }
        else
        {
            m_echo_req_received = false;
            toggle_counter = 0;
            LEDS_OFF(LED_TWO);
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
        {echo_req_indicate,       ECHO_REQ_BLINK_INTERVAL_MS},
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


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */


/**@brief Callback handler to receive data on UDP port.
 *
 * @param[in]   p_socket         Socket identifier.
 * @param[in]   p_ip_header      IPv6 header containing source and destination addresses.
 * @param[in]   p_udp_header     UDP header identifying local and remote endpoints.
 * @param[in]   process_result   Result of data reception, there could be possible errors like
 *                               invalid checksum etc.
 * @param[in]   iot_pbuffer_t    Packet buffer containing the received data packet.
 *
 * @retval      NRF_SUCCESS      Indicates received data was handled successfully, else an error
 *                               code indicating reason for failure.
 */
uint32_t rx_udp_port_app_handler(const udp6_socket_t * p_socket,
                                 const ipv6_header_t * p_ip_header,
                                 const udp6_header_t * p_udp_header,
                                 uint32_t              process_result,
                                 iot_pbuffer_t       * p_rx_packet)
{
    // APPL_LOG("[APPL]: Got UDP6 data on socket 0x%08lx\r\n", p_socket->socket_id);
    // APPL_LOG("[APPL]: Source IPv6 Address: ");
    // APPL_ADDR(p_ip_header->srcaddr);

    APP_ERROR_CHECK(process_result);

    // Print PORTs
    // APPL_LOG("[APPL]: UDP Destination port: %lx\r\n", HTONS(p_udp_header->destport));
    // APPL_LOG("[APPL]: UDP Source port: %lx\r\n",      HTONS(p_udp_header->srcport));

    // UDP packet received from the Client node.
    APPL_LOG("[APPL]: Received UDP packet sequence number: %ld\r\n", \
             uint32_decode(&p_rx_packet->p_payload[0]));

    iot_pbuffer_alloc_param_t   pbuff_param;
    iot_pbuffer_t             * p_tx_buffer;

    pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
    pbuff_param.type   = UDP6_PACKET_TYPE;
    pbuff_param.length = p_rx_packet->length;

    // Allocate packet buffer.
    uint32_t err_code = iot_pbuffer_allocate(&pbuff_param, &p_tx_buffer);
    APP_ERROR_CHECK(err_code);

    memcpy(p_tx_buffer->p_payload, p_rx_packet->p_payload, p_rx_packet->length);

    // Send back the received packet payload without any modification.
    err_code = udp6_socket_sendto(&m_udp_socket, &p_ip_header->srcaddr, p_udp_header->srcport, p_tx_buffer);
    APP_ERROR_CHECK(err_code);

    m_display_state  = LEDS_TX_UDP_PACKET;
    m_udp_tx_occured = true;

    return NRF_SUCCESS;
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

    APPL_LOG("[APPL]: Source IPv6 Address: ");
    APPL_ADDR(p_ip_header->srcaddr);
    APPL_LOG("[APPL]: Destination IPv6 Address: ");
    APPL_ADDR(p_ip_header->destaddr);
    APPL_LOG("[APPL]: Process result = 0x%08lx\r\n", process_result);

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
            m_display_state = LEDS_TX_ECHO_RESPONSE;
            if (m_echo_req_received == false)
            {
                LEDS_OFF(LED_TWO);
                m_echo_req_received = true;
            }

            break;
        case ICMP6_TYPE_ECHO_REPLY:
            APPL_LOG("[APPL]: ICMP6 Message Type = Echo Reply\r\n");
            break;
        case ICMP6_TYPE_ROUTER_SOLICITATION:
            APPL_LOG("[APPL]: ICMP6 Message Type = Router Solicitation\r\n");
            break;
        case ICMP6_TYPE_ROUTER_ADVERTISEMENT:
            APPL_LOG("[APPL]: ICMP6 Message Type = Router Advertisement\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_SOLICITATION:
            APPL_LOG("[APPL]: ICMP6 Message Type = Neighbor Solicitation\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_ADVERTISEMENT:
            APPL_LOG("[APPL]: ICMP6 Message Type = Neighbor Advertisement\r\n");
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
    uint32_t err_code;

    APPL_LOG("[APPL]: Got IP Application Handler Event on interface 0x%p\r\n", p_interface);

    switch(p_event->event_id)
    {
        case IPV6_EVT_INTERFACE_ADD:
#ifdef COMMISSIONING_ENABLED
            commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_STOP_RESET);
#endif // COMMISSIONING_ENABLED
            APPL_LOG("[APPL]: New interface added!\r\n");

            err_code = udp6_socket_allocate(&m_udp_socket);
            APP_ERROR_CHECK(err_code);

            err_code = udp6_socket_bind(&m_udp_socket, IPV6_ADDR_ANY, UDP_PORT);
            APP_ERROR_CHECK(err_code);

            err_code = udp6_socket_recv(&m_udp_socket, rx_udp_port_app_handler);
            APP_ERROR_CHECK(err_code);

            m_display_state  = LEDS_TX_ECHO_RESPONSE;

            break;
        case IPV6_EVT_INTERFACE_DELETE:
#ifdef COMMISSIONING_ENABLED
            commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED
            APPL_LOG("[APPL]: Interface removed!\r\n");

            err_code = udp6_socket_free(&m_udp_socket);
            APP_ERROR_CHECK(err_code);

            m_display_state  = LEDS_IPV6_IF_DOWN;

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
#ifdef COMMISSIONING_ENABLED
        {ERASE_BUTTON_PIN_NO, false, BUTTON_PULL, button_event_handler}
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
#endif // COMMISSIONING_ENABLED


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
    timers_init();

#ifdef COMMISSIONING_ENABLED
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    buttons_init();
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

    // Initialize IP Stack.
    ip_stack_init();

    // Initialize IoT timer module.
    iot_timer_init();

    APPL_LOG("\r\n");
    APPL_LOG("[APPL]: Init complete.\r\n");

    // Start execution.
    connectable_mode_enter();

    // Enter main loop.
    for (;;)
    {
        /* Sleep waiting for an application event. */
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

/**
 * @}
 */
