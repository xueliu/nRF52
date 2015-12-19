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
 * @defgroup iot_sdk_app_lwip_udp_server main.c
 * @{
 * @ingroup iot_sdk_app_lwip
 * @brief This file contains the source code for LwIP UDP Server sample application.
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "sdk_config.h"
#include "app_timer.h"
#include "iot_timer.h"
#include "app_button.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
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
/*lint -save -e607 Suppress warning 607 "Parameter p of macro found within string" */
#include "lwip/udp.h"
/*lint -restore */
#include "lwip/timers.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"
#include "ipv6_medium.h"

#define LED_ONE                             BSP_LED_0_MASK                                          /**< Is on when device is advertising. */
#define LED_TWO                             BSP_LED_1_MASK                                          /**< Is on when device is connected. */
#define DISPLAY_LED_0                       BSP_LED_0_MASK                                          /**< LED used for displaying mod 4 of data payload received on UDP port. */
#define DISPLAY_LED_1                       BSP_LED_1_MASK                                          /**< LED used for displaying mod 4 of data payload received on UDP port. */
#define LED_THREE                           BSP_LED_2_MASK
#define LED_FOUR                            BSP_LED_3_MASK
#define ALL_APP_LED                        (LED_ONE | LED_TWO | LED_THREE | LED_FOUR)               /**< Define used for simultaneous operation of all application LEDs. */

#ifdef COMMISSIONING_ENABLED
#define ERASE_BUTTON_PIN_NO                 BSP_BUTTON_3                                            /**< Button used to erase commissioning settings. */
#endif // COMMISSIONING_ENABLED

#define APP_TIMER_OP_QUEUE_SIZE             5
#define LWIP_SYS_TICK_MS                    100                                                     /**< Interval for timer used as trigger to send. */
#define LED_BLINK_INTERVAL_MS               300                                                     /**< LED blinking interval. */

#define DEAD_BEEF                           0xDEADBEEF                                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME                 128                                                     /**< Max length of filename to copy for the debug error handler. */

#define APPL_LOG                            app_trace_log                                           /**< Macro for logging application messages on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */
#define APPL_DUMP                           app_trace_dump                                          /**< Macro for dumping application data on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */

#define UDP_LISTEN_PORT                     9000                                                    /**< UDP listen port number. */
#define UDP_DATA_SIZE                       8                                                       /**< UDP Data size sent on remote. */

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                             /**< System Timer used to service CoAP and LWIP periodically. */
eui64_t                                     eui64_local_iid;                                        /**< Local EUI64 value that is used as the IID for*/
static ipv6_medium_instance_t               m_ipv6_medium;
static struct udp_pcb                     * mp_udp_port;                                            /**< UDP Port to listen on. */

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
        LEDS_OFF(ALL_APP_LED);
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


#ifdef COMMISSIONING_ENABLED
/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 */
static void blink_timeout_handler(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);
    static bool id_mode_previously_enabled;

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
}
#endif // COMMISSIONING_ENABLED


/**@brief Sends UDP data in Response format described in description above.
 *
 * @details Sends UDP data in Response of size 8 in format described in description above.
 *          Requested sequence number is used to send the response.
 *
 * @param[in] p_remote_addr   Remote address on which response is to be sent.
 * @param[in] port_number     Remote port number on which response is to be sent.
*  @param[in] sequence_number Sequence number to be used in the response.
 */
static void udp_data_send(ip6_addr_t * p_remote_addr, uint16_t port_number, uint32_t sequence_number)
{
    struct pbuf * p_send_buf;
    uint8_t     * p_udp_data;

    APPL_LOG ("[APPL]: >> UDP Data Tx\r\n");

    p_send_buf = pbuf_alloc (PBUF_TRANSPORT, UDP_DATA_SIZE, PBUF_RAM);
    ASSERT(p_send_buf != NULL);
    p_send_buf->len = UDP_DATA_SIZE;
    p_udp_data = p_send_buf->payload;

    p_udp_data[0] = (uint8_t )((sequence_number >> 24) & 0x000000FF);
    p_udp_data[1] = (uint8_t )((sequence_number >> 16) & 0x000000FF);
    p_udp_data[2] = (uint8_t )((sequence_number >> 8) & 0x000000FF);
    p_udp_data[3] = (uint8_t )(sequence_number & 0x000000FF);

    p_udp_data[4] = 'P';
    p_udp_data[5] = 'o';
    p_udp_data[6] = 'n';
    p_udp_data[7] = 'g';

    //Send UDP Data packet.
    err_t err = udp_sendto_ip6(mp_udp_port, p_send_buf, p_remote_addr, port_number);
    APP_ERROR_CHECK(err);

    UNUSED_VARIABLE(pbuf_free(p_send_buf));

    APPL_DUMP(p_send_buf->payload,p_send_buf->len);
    APPL_LOG ("[APPL]: << UDP Data Tx\r\n");
}


/**@brief Callback registered with UDP module to handle incoming data on the port.
 *
 * @details Callback registered with UDP module to handle incoming data on the port.
 *
 * @param[in] p_arg          Any arguments registered by application on the port.
 * @param[in] p_pcb          Identifies the UDP port on which data is received.
 * @param[in] p_buffer       Buffer containing data and its length.
 * @param[in] p_remote_addr  Address of the remote side from which data was received.
 * @param[in] remote_port    Remote port number used when sending the data to the application port.
 */
void udp_recv_data_handler(void              * p_arg,
                           struct udp_pcb    * p_pcb,
                           struct pbuf       * p_buffer,
                           ip6_addr_t        * p_remote_addr,
                           u16_t               remote_port)
{
    APPL_LOG ("[APPL]: >> UDP Data Rx on Port 0x%08X\r\n", remote_port);
    APPL_DUMP (p_buffer->payload,p_buffer->len);

    uint8_t *p_data = p_buffer->payload;

    if (p_buffer->len == UDP_DATA_SIZE)
    {
        uint32_t sequence_number = 0;

        sequence_number  = ((p_data[0] << 24) & 0xFF000000);
        sequence_number |= ((p_data[1] << 16) & 0x00FF0000);
        sequence_number |= ((p_data[2] << 8)  & 0x0000FF00);
        sequence_number |= (p_data[3]         & 0x000000FF);

        LEDS_OFF(ALL_APP_LED);

        if (sequence_number & 0x00000001)
        {
            LEDS_ON(DISPLAY_LED_0);
        }
        if (sequence_number & 0x00000002)
        {
            LEDS_ON(DISPLAY_LED_1);
        }

        //Send Response
        udp_data_send(p_remote_addr, remote_port, sequence_number);
    }
    else
    {
        APPL_LOG ("[APPL]: UDP data received in incorrect format.\r\n");
    }

    APPL_LOG ("[APPL]: << UDP Data Rx on Port 0x%08X\r\n", remote_port);
}


/**@brief UDP Port Set-Up.
 *
 * @details Sets up UDP Port to listen on.
 */
static void udp_port_setup(void)
{
    ip6_addr_t any_addr;
    ip6_addr_set_any(&any_addr);

    mp_udp_port = udp_new_ip6();

    if (mp_udp_port != NULL)
    {
        err_t err = udp_bind_ip6(mp_udp_port, &any_addr, UDP_LISTEN_PORT);
        APP_ERROR_CHECK(err);

        udp_recv_ip6(mp_udp_port,udp_recv_data_handler, NULL);
    }
    else
    {
        ASSERT(0);
    }
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
    return;
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


/**@brief Timer callback used for periodic servicing of LwIP protocol timers.
 *
 * @details Timer callback used for periodic servicing of LwIP protocol timers.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void system_timer_callback(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_VARIABLE(wall_clock_value);
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
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create a sys timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,
                                APP_TIMER_MODE_REPEATED,
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {system_timer_callback,   LWIP_SYS_TICK_MS},
#ifdef COMMISSIONING_ENABLED
        {blink_timeout_handler,   LED_BLINK_INTERVAL_MS},
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


/**@brief Function to handle interface up event. */
void nrf_driver_interface_up(void)
{
#ifdef COMMISSIONING_ENABLED
    commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_STOP_RESET);
#endif // COMMISSIONING_ENABLED

    APPL_LOG ("[APPL]: IPv6 interface up.\r\n");

    sys_check_timeouts();

    LEDS_OFF(LED_ONE);
    LEDS_ON(LED_TWO);
}


/**@brief Function to handle interface down event. */
void nrf_driver_interface_down(void)
{
#ifdef COMMISSIONING_ENABLED
    commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED

    APPL_LOG ("[APPL]: IPv6 interface down.\r\n");

    LEDS_ON(LED_ONE);
    LEDS_OFF(LED_TWO);
}


/**@brief Function for starting connectable mode.
 */
static void connectable_mode_enter(void)
{
    uint32_t err_code = ipv6_medium_connectable_mode_enter(m_ipv6_medium.ipv6_medium_instance_id);
    APP_ERROR_CHECK(err_code);

    APPL_LOG("[APPL]: Physical layer in connectable mode.\r\n");
    LEDS_OFF(LED_TWO);
    LEDS_ON(LED_ONE);
}


static void on_ipv6_medium_evt(ipv6_medium_evt_t * p_ipv6_medium_evt)
{
    switch (p_ipv6_medium_evt->ipv6_medium_evt_id)
    {
        case IPV6_MEDIUM_EVT_CONN_UP:
        {
            APPL_LOG("[APPL]: Physical layer: connected.\r\n");
            LEDS_OFF(LED_ONE);
            LEDS_ON(LED_TWO);
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

    ip_stack_init();
    udp_port_setup();

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
