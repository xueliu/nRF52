
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
 * @defgroup iot_sdk_app_tcp_client main.c
 * @{
 * @ingroup iot_sdk_app_lwip
 *
 * @brief This file contains the source code for LwIP TCP Client sample application.
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "sdk_config.h"
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
/*lint -save -e607 */
#include "lwip/tcp.h"
/*lint -restore */
#include "lwip/timers.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"
#include "iot_timer.h"
#include "ipv6_medium.h"

/** Remote TCP Port Address on which data is transmitted.
 *  Modify m_remote_addr according to your setup.
 *  The address provided below is a place holder.  */
static const ip6_addr_t                     m_remote_addr =
{
    .addr =
    {HTONL(0x20010DB8),
    0x00000000,
    0x00000000,
    HTONL(0x00000001)}
};

#define LED_ONE                             BSP_LED_0_MASK                                          /**< Is on when device is advertising. */
#define LED_TWO                             BSP_LED_1_MASK                                          /**< Is on when device is connected. */
#define LED_THREE                           BSP_LED_2_MASK
#define LED_FOUR                            BSP_LED_3_MASK
#define TCP_CONNECTED_LED                   BSP_LED_2_MASK                                          /**< Is on when a TCP connection is established. */
#define DISPLAY_LED_0                       BSP_LED_0_MASK                                          /**< LED used for displaying mod 4 of data payload received on TCP port. */
#define DISPLAY_LED_1                       BSP_LED_1_MASK                                          /**< LED used for displaying mod 4 of data payload received on TCP port. */
#define DISPLAY_LED_2                       BSP_LED_2_MASK                                          /**< LED used for displaying mod 4 of data payload received on TCP port. */
#define DISPLAY_LED_3                       BSP_LED_3_MASK                                          /**< LED used for displaying mod 4 of data payload received on TCP port. */
#define ALL_APP_LED                        (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                            BSP_LED_2_MASK | BSP_LED_3_MASK)                        /**< Define used for simultaneous operation of all application LEDs. */

#ifdef COMMISSIONING_ENABLED
#define ERASE_BUTTON_PIN_NO                 BSP_BUTTON_3                                            /**< Button used to erase commissioning settings. */
#endif // COMMISSIONING_ENABLED

#define APP_TIMER_OP_QUEUE_SIZE             5
#define LWIP_SYS_TICK_MS                    100                                                     /**< Interval for timer used as trigger to send. */
#define TX_INTERVAL_MS                      400                                                     /**< Interval between sending packets to the peer. */
#define LED_BLINK_INTERVAL_MS               300                                                     /**< LED blinking interval. */

#define SCHED_MAX_EVENT_DATA_SIZE           128                                                     /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE                    12                                                      /**< Maximum number of events in the scheduler queue. */

#define DEAD_BEEF                           0xDEADBEEF                                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME                 128                                                     /**< Max length of filename to copy for the debug error handler. */

#define APPL_LOG                            app_trace_log                                           /**< Macro for logging application messages on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */
#define APPL_DUMP                           app_trace_dump                                          /**< Macro for dumping application data on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */

#define TCP_SERVER_PORT                     9000                                                    /**< Remote server port to which this application requests a connection. */
#define TCP_CLIENT_PORT                     9001                                                    /**< Local client port number used by the applciation. */
#define TCP_DATA_SIZE                       8                                                       /**< TCP Data size sent on remote. */

typedef enum
{
    TCP_STATE_IDLE,
    TCP_STATE_REQUEST_CONNECTION,
    TCP_STATE_CONNECTED,
    TCP_STATE_DATA_TX_IN_PROGRESS,
    TCP_STATE_TCP_SEND_PENDING,
    TCP_STATE_DISCONNECTED
} tcp_state_t;

APP_TIMER_DEF(m_iot_timer_tick_src_id);                                                             /**< System Timer used to service CoAP and LWIP periodically. */
eui64_t                                     eui64_local_iid;                                        /**< Local EUI64 value that is used as the IID for*/
static ipv6_medium_instance_t               m_ipv6_medium;
static struct tcp_pcb                     * mp_tcp_port;                                            /**< TCP Port to listen on. */
static tcp_state_t                          m_tcp_state;                                            /**< TCP State information. */

static bool                                 m_is_ipv6_if_up = false;
static uint32_t                             m_sequence_number = 0;                                  /**< Counter used for sequencing data packets transmitted. */

static void tcp_request_connection(void);
static void tcp_send_data(struct tcp_pcb * p_pcb);

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

    if (button_action == APP_BUTTON_PUSH)
    {
        switch (pin_no)
        {
            case BSP_BUTTON_0:
            {
                tcp_request_connection();
                break;
            }
            case BSP_BUTTON_1:
            {
                if (m_tcp_state == TCP_STATE_CONNECTED)
                {
                    m_tcp_state = TCP_STATE_DATA_TX_IN_PROGRESS;
                }
                else if ((m_tcp_state == TCP_STATE_DATA_TX_IN_PROGRESS) || 
                         (m_tcp_state == TCP_STATE_TCP_SEND_PENDING))
                {
                    m_tcp_state = TCP_STATE_CONNECTED;
                }
                break;
            }
            default:
                break;
        }
    }
}


static void button_init(void)
{
    uint32_t err_code;

    // Configure HR_INC_BUTTON_PIN_NO and HR_DEC_BUTTON_PIN_NO as wake up buttons and also configure
    // for 'pull up' because the eval board does not have external pull up resistors connected to
    // the buttons.
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


/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}


/**@brief TCP Port Write complete callback.
 *
 * @details Calbback registered to be notified of the write complete event on the TCP port.
 *          In case write complete is notified with 'zero' length, port is closed.
 *
 * @param[in]   p_arg     Receive argument set on the port.
 * @param[in]   p_pcb     PCB identifier of the port.
 * @param[in]   len       Length of data written successfully.
 *
 * @retval ERR_OK.
 */
static err_t tcp_write_complete(void           * p_arg,
                                struct tcp_pcb * p_pcb,
                                u16_t            len)
{
    UNUSED_PARAMETER(p_arg);
    UNUSED_PARAMETER(p_pcb);
    
    if (m_tcp_state == TCP_STATE_TCP_SEND_PENDING)
    {
        m_tcp_state = TCP_STATE_DATA_TX_IN_PROGRESS;
    }

    return ERR_OK;
}


/**@brief Send test data on the port.
 *
  * @details Sends TCP data in Request of size 8 in format described in description above.
 *
 * @param[in]   p_pcb     PCB identifier of the port.
 */
static void tcp_send_data(struct tcp_pcb * p_pcb)
{
    err_t  err = ERR_OK;

    APPL_LOG ("[APPL]: >> TCP Tx Data.\r\n");
    
    if (m_tcp_state == TCP_STATE_TCP_SEND_PENDING)
    {
        return;
    }

    uint32_t len = tcp_sndbuf(p_pcb);

    APPL_LOG ("[APPL]: Available TCP length 0x%08lx\r\n", len);

    if (len >= TCP_DATA_SIZE) 
    {
        m_sequence_number++;

        //Register callback to get notification of data reception is complete.
        tcp_sent(p_pcb, tcp_write_complete);
        uint8_t tcp_data[TCP_DATA_SIZE];

        tcp_data[0] = (uint8_t )((m_sequence_number >> 24) & 0x000000FF);
        tcp_data[1] = (uint8_t )((m_sequence_number >> 16) & 0x000000FF);
        tcp_data[2] = (uint8_t )((m_sequence_number >> 8)  & 0x000000FF);
        tcp_data[3] = (uint8_t )(m_sequence_number         & 0x000000FF);

        tcp_data[4] = 'P';
        tcp_data[5] = 'i';
        tcp_data[6] = 'n';
        tcp_data[7] = 'g';

        //Enqueue data for transmission.
        err = tcp_write(p_pcb, tcp_data, TCP_DATA_SIZE, 1);

        if (err != ERR_OK)
        {
            APPL_LOG ("[APPL]: Failed to send TCP packet, reason %d\r\n", err);
            m_sequence_number--;
        }
        else
        {
            m_tcp_state = TCP_STATE_TCP_SEND_PENDING;
            APPL_LOG ("[APPL]: Data Tx, Sequence number 0x%08lx\r\n", m_sequence_number);
        }
    }
    else
    {
        APPL_LOG ("[APPL]: No room to send next request, available length 0x%08lx\r\n", len);
    }


    APPL_LOG ("[APPL]: << TCP Tx Data.\r\n");
}


/**@brief Callback registered for receiving data on the TCP Port.
 *
 * @param[in]   p_arg     Receive argument set on the TCP port.
 * @param[in]   p_pcb     TCP PCB on which data is received.
 * @param[in]   p_buffer  Buffer with received data.
 * @param[in]   err       Event result indicating error associated with the receive,
 *                        if any, else ERR_OK.
 */
err_t tcp_recv_data_handler(void           * p_arg,
                            struct tcp_pcb * p_pcb,
                            struct pbuf    * p_buffer,
                            err_t            err)
{
    APPL_LOG ("[APPL]: >> TCP Data.\r\n");

    //Check event result before proceeding.
    if (err == ERR_OK)
    {
        uint8_t *p_data = p_buffer->payload;

        if (p_buffer->len == TCP_DATA_SIZE)
        {
            uint32_t rx_seq_number = 0;

            rx_seq_number  = ((p_data[0] << 24) & 0xFF000000);
            rx_seq_number |= ((p_data[1] << 16) & 0x00FF0000);
            rx_seq_number |= ((p_data[2] << 8)  & 0x0000FF00);
            rx_seq_number |= (p_data[3]         & 0x000000FF);

            LEDS_OFF(ALL_APP_LED);

            if (rx_seq_number & 0x00000001)
            {
                LEDS_ON(DISPLAY_LED_0);
            }
            if (rx_seq_number & 0x00000002)
            {
                LEDS_ON(DISPLAY_LED_1);
            }
            if (rx_seq_number & 0x00000004)
            {
                LEDS_ON(DISPLAY_LED_2);
            }
            if (rx_seq_number & 0x00000008)
            {
                LEDS_ON(DISPLAY_LED_3);
            }

            if (rx_seq_number != m_sequence_number)
            {
                APPL_LOG ("[APPL]: Mismatch in sequence number.\r\n");
            }
        }
        // All is good with the data received, process it.
        tcp_recved(p_pcb, p_buffer->tot_len);
        UNUSED_VARIABLE(pbuf_free(p_buffer));
    }
    else
    {
        //Free the buffer in case its not NULL.
        if (p_buffer != NULL)
        {
            UNUSED_VARIABLE(pbuf_free(p_buffer));
        }
    }

    return ERR_OK;
}


/**@brief Callback registered To be notified of errors on the TCP ports.
 *
 * @param[in]   p_arg     Receive argument set on the port.
 * @param[in]   err       Indication of nature of error that occurred on the port.
 */
static void tcp_error_handler(void *p_arg, err_t err)
{
    LWIP_UNUSED_ARG(p_arg);
    LWIP_UNUSED_ARG(err);

    APPL_LOG ("[APPL]: Error on TCP port, reason 0x%08x\r\n", err);
}


/**@brief TCP Poll Callback.
 *
 * @details Callback registered for TCP port polling, currently does nothing.
 *
 * @param[in]   p_arg     Receive argument set on the port.
 * @param[in]   p_pcb     PCB identifier of the port.
 *
 * @retval ERR_OK.
 */
static err_t tcp_connection_poll(void *p_arg, struct tcp_pcb *p_pcb)
{
    LWIP_UNUSED_ARG(p_arg);
    LWIP_UNUSED_ARG(p_pcb);

    return ERR_OK;
}


/**@brief TCP Port Connection Callback.
 *
 * @details Callback registered with TCP for connection complete. err indicates if the
 * @param[in]   p_arg     Receive argument set on the port.
 * @param[in]   p_pcb     PCB identifier of the port.
 * @param[in]   err       Event result indicating error associated with the receive,
 *                        if any, else ERR_OK.
 */
static err_t tcp_connection_callback(void           * p_arg,
                                     struct tcp_pcb * p_pcb,
                                     err_t            err)
{
    APPL_LOG ("[APPL]: >> TCP Connected, result 0x%08X.\r\n", err);

    //Ensure connection establishment was successful.
    APP_ERROR_CHECK(err);

    //Set the state of TCP port and associated handlers/information.
    m_tcp_state = TCP_STATE_CONNECTED;

    tcp_setprio(p_pcb, TCP_PRIO_MIN);
    tcp_arg(p_pcb, NULL);
    tcp_recv(p_pcb, tcp_recv_data_handler);
    tcp_err(p_pcb, tcp_error_handler);
    tcp_poll(p_pcb, tcp_connection_poll, 0);

    LEDS_ON(TCP_CONNECTED_LED);

    return ERR_OK;
}


/**@brief Setup TCP Port.
 *
 * @details Set up TCP Port and register receive callback.
 */
static void tcp_port_setup(void)
{
    ip6_addr_t any_addr;
    ip6_addr_set_any(&any_addr);

    mp_tcp_port = tcp_new_ip6();

    if (mp_tcp_port != NULL)
    {
        err_t err = tcp_bind_ip6(mp_tcp_port, &any_addr, TCP_CLIENT_PORT);
        APP_ERROR_CHECK(err);
    }
    else
    {
        ASSERT(0);
    }
}


/**@brief Request TCP Port Connection.
 *
 * @details Request TCP Port Connection, connection procedure is complete when
 *          tcp_connected_callback called.
 */
void tcp_request_connection(void)
{
    err_t err = tcp_connect_ip6(mp_tcp_port, &m_remote_addr, TCP_SERVER_PORT, tcp_connection_callback);
    APP_ERROR_CHECK(err);
    APPL_LOG("[APPL]: >> TCP Connection Requested to remote addr.\r\n");
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


/**@brief Timer callback used for periodic servicing of LwIP protocol timers.
 *        This trigger is also used in the example to trigger sending TCP Connection.
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


/**@brief Timer callback used for periodic servicing of LwIP protocol timers.
 *        This trigger is also used in the example to trigger sending TCP Connection.
 *
 * @details Timer callback used for periodic servicing of LwIP protocol timers.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void tcp_request_timer_callback(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_VARIABLE(wall_clock_value);

    if ((m_is_ipv6_if_up == true) && (m_tcp_state == TCP_STATE_DATA_TX_IN_PROGRESS))
    {
        tcp_send_data(mp_tcp_port);
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

    // Initialize timer module.
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, true);

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
        {system_timer_callback,      LWIP_SYS_TICK_MS},
        {tcp_request_timer_callback, TX_INTERVAL_MS},
#ifdef COMMISSIONING_ENABLED
        {blink_timeout_handler,      LED_BLINK_INTERVAL_MS},
        {commissioning_time_tick,    SEC_TO_MILLISEC(COMMISSIONING_TICK_INTERVAL_SEC)}
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
    m_is_ipv6_if_up = true;

#ifdef COMMISSIONING_ENABLED
    commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_STOP_RESET);
#endif // COMMISSIONING_ENABLED

    APPL_LOG ("[APPL]: IPv6 interface up.\r\n");

    sys_check_timeouts();

    m_tcp_state = TCP_STATE_REQUEST_CONNECTION;

    LEDS_OFF(LED_ONE);
    LEDS_ON(LED_TWO);
}


/**@brief Function to handle interface down event. */
void nrf_driver_interface_down(void)
{
    m_is_ipv6_if_up = false;

#ifdef COMMISSIONING_ENABLED
    commissioning_joining_mode_timer_ctrl(JOINING_MODE_TIMER_START);
#endif // COMMISSIONING_ENABLED

    APPL_LOG ("[APPL]: IPv6 interface down.\r\n");

    LEDS_OFF((DISPLAY_LED_0 | DISPLAY_LED_1 | DISPLAY_LED_2 | DISPLAY_LED_3));
    LEDS_ON(LED_ONE);

    m_tcp_state = TCP_STATE_DISCONNECTED;
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
    scheduler_init();
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

    ip_stack_init ();
    tcp_port_setup();

    APPL_LOG ("\r\n");
    APPL_LOG ("[APPL]: Init done.\r\n");

    //Start execution.
    connectable_mode_enter();

    //Enter main loop.
    for (;;)
    {
        //Execute event schedule.
        app_sched_execute();

        //Sleep waiting for an application event.
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

/**
 * @}
 */
