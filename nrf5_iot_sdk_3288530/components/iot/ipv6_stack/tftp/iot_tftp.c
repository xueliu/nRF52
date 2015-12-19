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

#include "sdk_config.h"
#include "app_trace.h"
#include "iot_tftp.h"
#include "iot_common.h"
#include "udp_api.h"
#include "app_util.h"

/**
 * @defgroup iot_sdk_tftp Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests. These are intended for debugging purposes and
 *          can be enabled by defining the TFTP_ENABLE_LOGS.
 * @note That if ENABLE_DEBUG_LOG_SUPPORT is disabled, having TFTP_ENABLE_LOGS has no effect.
 * @{
 */
#if (TFTP_ENABLE_LOGS == 1)

#define TFTP_TRC  app_trace_log                                                                     /**< Used for logging details. */
#define TFTP_ERR  app_trace_log                                                                     /**< Used for logging errors in the module. */
#define TFTP_DUMP app_trace_dump                                                                    /**< Used for dumping octet information to get details of bond information etc. */

#else  // TFTP_ENABLE_LOGS

#define TFTP_TRC(...)                                                                               /**< Diasbles detailed logs. */
#define TFTP_ERR(...)                                                                               /**< Diasbles error logs. */
#define TFTP_DUMP(...)                                                                              /**< Diasbles dumping of octet streams. */

#endif  // TFTP_ENABLE_LOGS
/** @} */

/**
 * @defgroup tftp_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case need arises to use an alternative architecture.
 * @{
 */
#define TFTP_MUTEX_LOCK()         SDK_MUTEX_LOCK(m_tftp_mutex)                                      /**< Lock module using mutex. */
#define TFTP_MUTEX_UNLOCK()       SDK_MUTEX_UNLOCK(m_tftp_mutex)                                    /**< Unlock module using mutex. */
/** @} */

#define TFTP_HEADER_SIZE          2                                                                 /**< uint16_t opcode number. */
#define TFTP_BLOCK_ID_SIZE        2                                                                 /**< uint16_t block id number. */
#define TFTP_ERR_CODE_SIZE        2                                                                 /**< uint16_t error code. */
#define TFTP_DEFAULT_BLOCK_SIZE   512                                                               /**< uint16_t default data block size. */
#define TFTP_DEFAULT_PORT         69                                                                /**< uint16_t default TFTP server port number. */

/**@brief Supported TFTP options. */
#define OPTION_MODE_ASCII         "netascii"                                                        /**< NETASCII mode string defined inside RFC1350. */
#define OPTION_MODE_OCTET         "octet"                                                           /**< OCTET mode string defined inside RFC1350. */
#define OPTION_BLKSIZE            "blksize"                                                         /**< Block Size optoin string defined inside RFC2348. */
#define OPTION_TIMEOUT            "timeout"                                                         /**< Timeout option string defined inside RFC2349. */
#define OPTION_SIZE               "tsize"                                                           /**< Transfer Size option string defined inside RFC2348. */

#define NEXT_RETR_MAX_LENGTH      4                                                                 /**< Maximum length of TFTP "timeout" option value. */
#define BLKSIZE_MAX_LENGTH        10                                                                /**< Maximum length of TFTP "blksize" option value. */
#define FILE_SIZE_MAX_LENGTH      10                                                                /**< Maximum length of TFTP "tsize" option value. */

#define OPTION_ERROR_MESSAGE      "Unsupported option(s) requested"
#define UDP_ERROR_MSG             "UDP Error!"
#define LENGTH_ERROR_MSG          "Invalid packet length!"
#define UNINT_ERROR_MSG           "Connection reset by peer"
#define ACCESS_ERROR_MSG          "Access denied (cannot read/write from file)"
#define OPTION_SIZE_REQUEST_VALUE "0"

/**@brief TFTP Error codes. */
#define ERR_UNDEFINED              0                                                                /**< Not defined, see error message (if any). */
#define ERR_FILE_NOT_FOUND         1                                                                /**< File not found. */
#define ERR_ACCESS_ERROR           2                                                                /**< Access violation. */
#define ERR_STORAGE_FULL           3                                                                /**< Disk full or allocation exceeded. */
#define ERR_INVALID_OP             4                                                                /**< Illegal TFTP operation. */
#define ERR_INVALID_TID            5                                                                /**< Unknown transfer ID. */
#define ERR_FILE_EXISTS            6                                                                /**< File already exists. */
#define ERR_BAD_USER               7                                                                /**< No such user. */
#define ERR_OPTION_REJECT          8                                                                /**< Reject proposed options. */

/**@brief TFTP opcode's. This field specifies type of packet. */
#define TYPE_RRQ                   1                                                                /**< Read request (RRQ). */
#define TYPE_WRQ                   2                                                                /**< Write request (WRQ). */
#define TYPE_DATA                  3                                                                /**< Data (DATA). */
#define TYPE_ACK                   4                                                                /**< Acknowledgement (ACK). */
#define TYPE_ERR                   5                                                                /**< Error (ERROR). */
#define TYPE_OACK                  6                                                                /**< Option Acknowledgement (RRQ/WRQ ACK). */

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          DNS6_DISABLE_API_PARAM_CHECK should be set to 0 to enable these checks.
 *
 * @{
 */

#if (TFTP_DISABLE_API_PARAM_CHECK == 0)

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM)                      \
    if ((PARAM) == NULL)                             \
    {                                                \
        return (NRF_ERROR_NULL | IOT_TFTP_ERR_BASE); \
    }

#else // TFTP_DISABLE_API_PARAM_CHECK

#define NULL_PARAM_CHECK(PARAM)

#endif // DNS6_DISABLE_API_PARAM_CHECK

/**@brief Check err_code, free p_buffer and return on error. */
#define PBUFFER_FREE_IF_ERROR(err_code)         \
    if (err_code != NRF_SUCCESS)                \
    {                                           \
        (void)iot_pbuffer_free(p_buffer, true); \
        return err_code;                        \
    }

/**@brief Convert TFTP error code into IOT error with appropriate base. */
#define CONVERT_TO_IOT_ERROR(error_code)             \
    ((IOT_TFTP_ERR_BASE+0x0040) + NTOHS(error_code))

/**@brief Convert IOT error into TFTP error code by removing TFTP error base. */
#define CONVERT_TO_TFTP_ERROR(error_code)            \
    (HTONS(err_code - (IOT_TFTP_ERR_BASE+0x0040)))

/**@brief Iterator for string list delimited with '\0'. */
typedef struct
{
    char * p_start;                                                                                 /**< Pointer to the beginning of a string. */
    char * p_end;                                                                                   /**< Pointer to the end of a string. */
    struct curr_struct
    {
        char * p_key;                                                                               /**< Pointer to the last, found key string. */
        char * p_value;                                                                             /**< Pointer to the last, found value string. */
    } curr;
} option_iter_t;

/**@brief Allowed states of a single TFTP instance. */
typedef enum
{
    STATE_FREE = 0,                                                                                 /**< Start state, after calling UDP to allocate socket. */
    STATE_IDLE,                                                                                     /**< Socket is allocated, but not used. */
    STATE_CONNECTING_RRQ,                                                                           /**< RRQ packet sent. Waiting for response. */
    STATE_CONNECTING_WRQ,                                                                           /**< WRQ packet sent. Waiting for response. */
    STATE_SENDING,                                                                                  /**< Sending file and receiving ACK. */
    STATE_SEND_HOLD,                                                                                /**< Sending held. Waiting for resume call. */
    STATE_RECEIVING,                                                                                /**< Receiving file and sending ACK. */
    STATE_RECV_HOLD,                                                                                /**< Receiving held. Waiting for resume call. */
    STATE_RECV_COMPLETE                                                                             /**< State after receiving last DATA, before sending last ACK packet. There won't be another UDP event to emit IOT_TFTP_EVT_TRANSFER_GET_COMPLETE event, so next resume() should emit that event. */
} tftp_state_t;

/**@brief Internal TFTP instance structure. */
typedef struct
{
    iot_tftp_trans_params_t           init_params;                                                  /**< Connection parameters set during initialization. */
    iot_tftp_trans_params_t           connect_params;                                               /**< Negotiated Connection parameters. */
    udp6_socket_t                     socket;                                                       /**< UDP socket assigned to single instance. */
    tftp_state_t                      state;                                                        /**< Integer representing current state of an instance. */
    iot_tftp_callback_t               callback;                                                     /**< User defined callback (passed inside initial parameters structure). */
    iot_file_t                      * p_file;                                                       /**< Pointer to destination/source file assigned in get/put call. */
    uint16_t                          block_id;                                                     /**< ID of last received/sent data block. */
    uint16_t                          src_tid;                                                      /**< UDP port used for sending information to the server. */
    uint16_t                          dst_tid;                                                      /**< UDP port on which all packets will be sent. At first - dst_port (see below), then reassigned. */
    uint16_t                          dst_port;                                                     /**< UDP port on which request packets will be sent. Usually DEFAULT_PORT. */
    const char                      * p_password;                                                   /**< Pointer to a constant string containing password passed inside Read/Write Requests. */
    ipv6_addr_t                       addr;                                                         /**< IPv6 server address. */
    iot_pbuffer_t                   * p_packet;                                                     /**< Reference to the temporary packet buffer. */
    uint8_t                           retries;                                                      /**< Number of already performed retries. */
    volatile iot_timer_time_in_ms_t   request_timeout;                                              /**< Number of milliseconds on which last request should be retransmitted. */
} tftp_instance_t;

SDK_MUTEX_DEFINE(m_tftp_mutex)                                                                      /**< Mutex variable. Currently unused, this declaration does not occupy any space in RAM. */
static tftp_instance_t m_instances[TFTP_MAX_INSTANCES];                                             /**< Array of allowed TFTP instances. */

/**@brief Function for finding free TFTP instance index.
 *
 * @param[out] p_index  Index being found.
 *
 * @retval NRF_SUCCESS if passed instance was found, else NRF_ERROR_NO_MEM error code will
 *                     be returned.
 */
static uint32_t find_free_instance(uint32_t * p_index)
{
    uint32_t index = 0;

    for (index = 0; index < TFTP_MAX_INSTANCES; index++)
    {
        if (m_instances[index].state == STATE_FREE)
        {
            *p_index = index;

            return NRF_SUCCESS;
        }
    }

    return (NRF_ERROR_NO_MEM | IOT_TFTP_ERR_BASE);
}

/**@brief Function for resolving instance index by passed pointer.
 *
 * @param[in]  p_tftp   Pointer representing TFTP instance in user space.
 * @param[out] p_index  Index of passed TFTP instance.
 *
 * @retval NRF_SUCCESS if passed instance was found, else NRF_ERROR_INVALID_PARAM error code
 *                     will be returned.
 */
static uint32_t find_instance(iot_tftp_t * p_tftp, uint32_t * p_index)
{
    if (*p_tftp > TFTP_MAX_INSTANCES)
    {
        return (NRF_ERROR_INVALID_PARAM | IOT_TFTP_ERR_BASE);
    }

    *p_index = *p_tftp;

    return NRF_SUCCESS;
}

/**@brief Function for notifying application of the TFTP events.
 *
 * @param[in] p_tftp          TFTP instance.
 * @param[in] p_evt           Event description.
 *
 * @retval None.
 */
static void app_notify(iot_tftp_t * p_tftp, iot_tftp_evt_t * p_evt)
{
    uint32_t index;
    uint32_t err_code;

    err_code = find_instance(p_tftp, &index);
    if (err_code != NRF_SUCCESS)
    {
        return;
    }

    if (m_instances[index].callback)
    {
        TFTP_MUTEX_UNLOCK();


        // Call handler of user request.
        m_instances[index].callback(p_tftp, p_evt);

        TFTP_MUTEX_LOCK();
    }
}

/**@brief Increment option iterator.
 *
 * @details The iterator will point to the next option or to p_end if it reaches the end.
 *
 * @param[in]  p_iter   Pointer to option iterator.
 *
 * @retval NRF_SUCCESS if iterator successfully moved to next option, else an error code indicating reason
 *                     for failure.
 */
static uint32_t op_get_next(option_iter_t * p_iter)
{
    uint32_t key_length;
    uint32_t value_length;

    NULL_PARAM_CHECK(p_iter->p_start);
    NULL_PARAM_CHECK(p_iter->p_end);
    NULL_PARAM_CHECK(p_iter->curr.p_key);
    NULL_PARAM_CHECK(p_iter->curr.p_value);

    // If reached end.
    if ((p_iter->curr.p_value == p_iter->p_end) || (p_iter->curr.p_key == p_iter->p_end))
    {
        return (NRF_ERROR_DATA_SIZE | IOT_TFTP_ERR_BASE);
    }

    key_length   = strlen(p_iter->curr.p_key);
    value_length = strlen(p_iter->curr.p_value);

    if ((p_iter->curr.p_value == p_iter->p_start) && (p_iter->curr.p_key == p_iter->p_start))
    {
        // First call. Check if [start] + [string] fits before [end] reached.
        // This statement just checks if there is '\0' between start and end (passing single string as input).
        if (p_iter->curr.p_key + key_length < p_iter->p_end)
        {
            p_iter->curr.p_value = p_iter->curr.p_key + key_length + 1;

            return NRF_SUCCESS;
        }
        else
        {
            return (NRF_ERROR_DATA_SIZE | IOT_TFTP_ERR_BASE);
        }
    }
    else if (p_iter->curr.p_value + value_length < p_iter->p_end)
    {
        p_iter->curr.p_key   = p_iter->curr.p_value + value_length + 1;
        p_iter->curr.p_value = p_iter->curr.p_key + strlen(p_iter->curr.p_key) + 1;

        if ((*p_iter->curr.p_key == '\0') || (*p_iter->curr.p_value == '\0')) // If string list finishes before the end of the buffer.
        {
            p_iter->curr.p_key   = p_iter->p_end;
            p_iter->curr.p_value = p_iter->p_end;

            return (NRF_ERROR_DATA_SIZE | IOT_TFTP_ERR_BASE);
        }

        return NRF_SUCCESS;
    }
    else
    {
        p_iter->curr.p_key = p_iter->p_end;
        p_iter->curr.p_value = p_iter->p_end;

        return (NRF_ERROR_DATA_SIZE | IOT_TFTP_ERR_BASE);
    }
}

/**@brief Set new (key, value) pair at the end of a string.
 *
 * @param[out] p_iter       Pointer to iterator, which will be used to add (key, value) pair.
 * @param[in]  p_inp_key    Pointer to the new key string. If p_key is NULL, then this function will insert just value.
 * @param[in]  p_inp_value  Pointer to the new value string.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static __INLINE uint32_t op_set(option_iter_t * p_iter, const char * p_inp_key, const char * p_inp_value)
{
    char * p_last_key;
    char * p_last_value;

    NULL_PARAM_CHECK(p_iter->p_start);
    NULL_PARAM_CHECK(p_iter->p_end);
    NULL_PARAM_CHECK(p_iter->curr.p_key);
    NULL_PARAM_CHECK(p_iter->curr.p_value);

    p_last_key   = p_iter->curr.p_key;
    p_last_value = p_iter->curr.p_value;

    // Print appropriate trace log.
    if (p_inp_key != NULL)
    {
        TFTP_TRC("[TFTP]: Set option: %s with value: %s.\r\n", p_inp_key, p_inp_value);
    }
    else
    {
        TFTP_TRC("[TFTP]: Set value: %s.\r\n", p_inp_value);
    }

    // Set key & value pointers.
    if ((p_iter->curr.p_key == p_iter->p_start) && (p_iter->curr.p_value == p_iter->p_start)) // Start condition.
    {
        if (p_inp_key != NULL)
        {
            p_iter->curr.p_value = p_iter->curr.p_key + strlen(p_inp_key) + 1;
        }
        else
        {
            p_iter->curr.p_value = p_iter->curr.p_key;                                        // Insert only passed value.
            p_iter->curr.p_key   = p_iter->curr.p_value + strlen(p_iter->curr.p_value) + 1;   // Just assign anything different that p_start and inside buffer.
        }
    }
    else
    {
        p_iter->curr.p_key = p_iter->curr.p_value + strlen(p_iter->curr.p_value) + 1;         // New key starts where last value ends.

        if (p_inp_key != NULL)
        {
            p_iter->curr.p_value = p_iter->curr.p_key + strlen(p_inp_key) + 1;                // If key not null - new value starts where new key ends.
        }
        else
        {
            p_iter->curr.p_value = p_iter->curr.p_key;                                        // Otherwise - value is placed at the key position.
        }
    }

    // Copy strings into set pointers.
    if ((p_iter->curr.p_value + strlen(p_inp_value)) < p_iter->p_end)
    {
        if (p_inp_key != NULL)
        {
            memcpy(p_iter->curr.p_key, p_inp_key, strlen(p_inp_key) + 1);
        }
        memcpy(p_iter->curr.p_value, p_inp_value, strlen(p_inp_value) + 1);
    }
    else                                                                                      // If it is not possible to insert new key & value pair.
    {
        p_iter->curr.p_key   = p_last_key;
        p_iter->curr.p_value = p_last_value;

        TFTP_TRC("[TFTP]: Unable to set option (size ERROR!).\r\n");

        return (NRF_ERROR_DATA_SIZE | IOT_TFTP_ERR_BASE);
    }

    return NRF_SUCCESS;
}

/**@brief Initializes new option interator.
 *
 * @param[out] p_iter   Pointer to iterator, which will be configured.
 * @param[in]  p_buf    Pointer to the new string buffer which iterator will be modifying.
 * @param[in]  buf_len  Length of passed buffer.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static __INLINE void op_init(option_iter_t * p_iter, char * p_buf, uint32_t buf_len)
{
    p_iter->p_start      = p_buf;
    p_iter->p_end        = p_buf + buf_len;
    p_iter->curr.p_key   = p_buf;
    p_iter->curr.p_value = p_buf;
}

/**@brief: Converts string containing unsigned number into uint32_t.
 *
 * @param[in] p_str   Input string.
 *
 * @retval Integer number equal to read value. Reading process skips all non-digit characters.
 */
static uint32_t str_to_uint(char * p_str)
{
    uint32_t len;
    uint32_t ret_val = 0;
    uint32_t mul     = 1;

    if (p_str == NULL)
    {
        return 0;
    }

    len = strlen(p_str);

    while (len)
    {
        len--;

        if ((p_str[len] >= '0') && (p_str[len] <= '9')) // Skip unsupported characters.
        {
            ret_val += mul * (p_str[len] - '0');
            mul *= 10;
        }
    }

    return ret_val;
}

/**@brief: Converts unsigned number into string.
 *
 * @param[in]  number   Input number.
 * @param[out] p_str    Pointer to the output string.
 * @param[in]  len      Length of the passed output string buffer.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t uint_to_str(uint32_t number, char * p_str, uint16_t len)
{
    uint32_t i    = 0;
    uint32_t temp = number;

    if (len == 0)
    {
        return NRF_ERROR_INVALID_LENGTH;
    }

    // Check how many characters will be needed.
    if (temp == 0)
    {
        i = 1;
    }

    while(temp)
    {
        i++;
        temp /= 10;
    }

    // Set null character and check length.
    if (i + 1 > len)
    {
        p_str[0] = '\0';

        return NRF_ERROR_INVALID_LENGTH;
    }

    p_str[i] = '\0';

    // Set digits.
    while (i--)
    {
        p_str[i] = '0' + number % 10;
        number  /= 10;
    }

    return NRF_SUCCESS;
}

/**@breif Compare strings in a case insensitive way.
 *
 * @param[in] p_str1   Pointer to the first string.
 * @param[in] p_str2   Pointer to the second String.
 *
 * @retval If strings are equal returns 0, otherwise number of common characters.
 */
static uint32_t strcmp_ci(char * p_str1, char* p_str2)
{
    uint32_t min_len = 0;
    uint32_t str1_len;
    uint32_t str2_len;
    uint32_t i = 0;

    str1_len = strlen(p_str1);
    str2_len = strlen(p_str2);

    min_len = str1_len;

    if (str2_len < str1_len)
    {
        min_len = str2_len;
    }

    for (i = 0; i < min_len; i++)
    {
        char c1 = ((p_str1[i] >= 'a' && p_str1[i] <= 'z') ? p_str1[i] + 'A' - 'a' : p_str1[i]);
        char c2 = ((p_str2[i] >= 'a' && p_str2[i] <= 'z') ? p_str2[i] + 'A' - 'a' : p_str2[i]);

        if (c1 != c2)
        {
            return i + 1;
        }
    }

    if (str1_len != str2_len)
    {
        return i + 1;
    }

    return 0;
}

/**@brief Allocates p_buffer and fills in common fields.
 *
 * @param[in]  type         First field describing packet type.
 * @param[in]  id           Second field (Block ID / Error Code).
 * @param[out] pp_buffer    Sets pointer to the newly allocated buffer.
 * @param[in]  payload_len  Length of payload (additional fields / data).
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t compose_packet(uint16_t         type,
                               uint16_t         id,
                               iot_pbuffer_t ** pp_buffer,
                               uint32_t         payload_len)
{
    uint32_t                    err_code;
    iot_pbuffer_alloc_param_t   buffer_param;
    iot_pbuffer_t             * p_buffer;
    uint32_t                    byte_index;

    memset(&buffer_param, 0, sizeof(iot_pbuffer_alloc_param_t));
    buffer_param.length = TFTP_HEADER_SIZE + TFTP_BLOCK_ID_SIZE + payload_len;
    buffer_param.type   = UDP6_PACKET_TYPE;
    buffer_param.flags  = PBUFFER_FLAG_DEFAULT;

    err_code = iot_pbuffer_allocate(&buffer_param, &p_buffer);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    memset(p_buffer->p_payload, 0, buffer_param.length);
    byte_index = 0;

    // Insert type opcode.
    byte_index += uint16_encode(HTONS(type), &p_buffer->p_payload[byte_index]);

    if (type == TYPE_ERR)
    {
        // Insert err code.
        byte_index += uint16_encode(CONVERT_TO_TFTP_ERROR(id), &p_buffer->p_payload[byte_index]);
    }
    else
    {
        // Insert block ID.
        byte_index += uint16_encode(HTONS(id), &p_buffer->p_payload[byte_index]);
    }

    *pp_buffer = p_buffer;

    return NRF_SUCCESS;
}

/**@brief Reset instance request timer.
 *
 * @param[in] index  Index of pending instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t retr_timer_reset(uint32_t index)
{
    uint32_t               err_code;
    iot_timer_time_in_ms_t wall_clock_value;

    // Get wall clock time.
    err_code = iot_timer_wall_clock_get(&wall_clock_value);

    if (err_code == NRF_SUCCESS)
    {
        m_instances[index].request_timeout = wall_clock_value + m_instances[index].connect_params.next_retr * 1000;
    }

    return err_code;
}

/**@brief Function for checking if retransmission time of TFTP instance request has been expired.
 *
 * @param[in] index  Index of pending instance.
 *
 * @retval True if timer has been expired, False otherwise.
 */
static bool instance_timer_is_expired(uint32_t index)
{
    uint32_t               err_code;
    iot_timer_time_in_ms_t wall_clock_value;

    // Get wall clock time.
    err_code = iot_timer_wall_clock_get(&wall_clock_value);

    if (err_code == NRF_SUCCESS)
    {
        if (wall_clock_value >= m_instances[index].request_timeout)
        {
            return true;
        }
    }

    return false;
}

/**@brief Sets all instance values to defaults. */
static void instance_reset(uint32_t index)
{
    m_instances[index].state                     = STATE_FREE;
    m_instances[index].init_params.next_retr     = 0;
    m_instances[index].init_params.block_size    = TFTP_DEFAULT_BLOCK_SIZE;
    m_instances[index].connect_params.next_retr  = 0;
    m_instances[index].connect_params.block_size = TFTP_DEFAULT_BLOCK_SIZE;
    m_instances[index].p_file                    = NULL;
    m_instances[index].block_id                  = 0;
    m_instances[index].p_packet                  = NULL;
    m_instances[index].dst_port                  = TFTP_DEFAULT_PORT;
    m_instances[index].dst_tid                   = TFTP_DEFAULT_PORT;
    m_instances[index].retries                   = 0;
    m_instances[index].request_timeout           = 0;
    m_instances[index].callback                  = NULL;
    m_instances[index].src_tid                   = 0;
    m_instances[index].p_password                = NULL;
    memset(&m_instances[index].addr, 0, sizeof(ipv6_addr_t));
    memset(&m_instances[index].socket, 0, sizeof(udp6_socket_t));
}

/**@brief This function creeates error packet for specified TFTP instance.
 *
 * @param[in] index      Index of TFTP instance.
 * @param[in] p_err_evt  Event data structure (message and code).
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t send_err_msg(uint32_t index, iot_tftp_evt_err_t * p_err_evt)
{
    iot_pbuffer_t * p_buffer;
    uint8_t       * p_resp_packet;
    uint32_t        msg_len    = 0;
    uint16_t        byte_index = 0;
    uint32_t        err_code;

    TFTP_TRC("[TFTP]: Send ERROR packet. \r\n");

    if (p_err_evt->p_msg != NULL)
    {
        msg_len = strlen(p_err_evt->p_msg) + 1;
    }

    err_code = compose_packet(TYPE_ERR, p_err_evt->code, &p_buffer, msg_len);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    p_resp_packet = p_buffer->p_payload;
    byte_index    = TFTP_HEADER_SIZE + TFTP_ERR_CODE_SIZE;

    if (p_err_evt->p_msg != NULL)
    {
        memcpy(&p_resp_packet[byte_index], p_err_evt->p_msg, msg_len);
        byte_index += msg_len;
    }

    p_buffer->length = byte_index;

    TFTP_TRC("[TFTP]: Send packet to UDP module. \r\n");

    UNUSED_VARIABLE(retr_timer_reset(index));

    err_code = udp6_socket_sendto(&m_instances[index].socket,
                                  &m_instances[index].addr,
                                  m_instances[index].dst_tid,
                                  p_buffer);

    TFTP_TRC("[TFTP]:     Recv code: %08lx.\r\n", err_code);

    return err_code;
}

/**@brief This function creeates error packet for TID error, not being found.
 *
 * @param[in] p_socket   Socket from which error message is sent.
 * @param[in] p_addr     IPv6 Address to where error message is sent.
 * @param[in] tid        Erronous TID from the sender.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t send_err_tid(const udp6_socket_t * p_socket, const ipv6_addr_t * p_addr, uint16_t tid)
{
    iot_pbuffer_t * p_buffer;
    uint32_t        msg_len    = 0;
    uint16_t        byte_index = 0;
    uint32_t        err_code;

    TFTP_TRC("[TFTP]: Send TID ERROR packet. \r\n");

    err_code = compose_packet(TYPE_ERR, ERR_INVALID_TID, &p_buffer, msg_len);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    byte_index       = TFTP_HEADER_SIZE + TFTP_ERR_CODE_SIZE;
    p_buffer->length = byte_index;

    TFTP_TRC("[TFTP]: Send packet to UDP module. \r\n");

    err_code = udp6_socket_sendto(p_socket, p_addr, tid, p_buffer);

    TFTP_TRC("[TFTP]:     Recv code: %08lx.\r\n", err_code);

    return err_code;
}

/**@brief Sends ACK or next data chunk (block) after calling user callback or when hold timer expires.
 *
 * @param[in] p_tftp   Pointer to the TFTP instance (from user space).
 * @param[in] p_evt    Pointer to the event structure. Used for sending error messages.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t send_response(iot_tftp_t * p_tftp)
{
    uint32_t index;
    uint32_t err_code;

    TFTP_TRC("[TFTP]: Send packet.\r\n");

    err_code = find_instance(p_tftp, &index);
    if (err_code != NRF_SUCCESS)
    {
        TFTP_TRC("[TFTP]: Cannot find instance (send)!\r\n");

        return err_code;
    }

    switch (m_instances[index].state)
    {
        case STATE_IDLE:
            // Server should go to the CONNECTING state (request received).
            TFTP_TRC("[TFTP]: Inside IDLE state (send). \r\n");

            return NRF_SUCCESS;

        case STATE_SENDING:
        case STATE_RECEIVING:
        case STATE_SEND_HOLD:
        case STATE_RECV_HOLD:
        case STATE_RECV_COMPLETE:
            // Send DATA/ACK packet.
            TFTP_TRC("[TFTP]: Send packet to UDP module. \r\n");

            UNUSED_VARIABLE(retr_timer_reset(index));
            err_code = udp6_socket_sendto(&m_instances[index].socket,
                                          &m_instances[index].addr,
                                          m_instances[index].dst_tid,
                                          m_instances[index].p_packet);
            TFTP_TRC("[TFTP]:     Recv code: %08lx.\r\n", err_code);

            return err_code;

        default:
            TFTP_TRC("[TFTP]: Invalid state (send)!\r\n");

            return (NRF_ERROR_INVALID_STATE | IOT_TFTP_ERR_BASE);
    }
}



/**@brief Aborts TFTP client ongoing procedure.
 *
 * @param[in] index  Index of TFTP instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
void instance_abort(uint32_t index)
{
    uint32_t internal_err;

    switch(m_instances[index].state)
    {
        case STATE_SEND_HOLD:
        case STATE_RECV_HOLD:
            // Free pbuffer.
            internal_err = iot_pbuffer_free(m_instances[index].p_packet, true);
            if(internal_err != NRF_SUCCESS)
            {
                TFTP_ERR("[TFTP]: Cannot free pbuffer - %p\r\n", m_instances[index].p_packet);
            }

            // Close file.
            internal_err = iot_file_fclose(m_instances[index].p_file);
            if(internal_err != NRF_SUCCESS)
            {
                TFTP_ERR("[TFTP]: Cannot close file - %p\r\n", m_instances[index].p_file);
            }

            break;
        case STATE_SENDING:
        case STATE_RECEIVING:
            // Close file.
            internal_err = iot_file_fclose(m_instances[index].p_file);
            if(internal_err != NRF_SUCCESS)
            {
                TFTP_ERR("[TFTP]: Cannot close file - %p\r\n", m_instances[index].p_file);
            }
            break;
        case STATE_CONNECTING_RRQ:
        case STATE_CONNECTING_WRQ:
        case STATE_IDLE:
        case STATE_FREE:
        default:
            break;
    }

    TFTP_TRC("[TFTP]: Reset instance %ld.\r\n", index);

    m_instances[index].state           = STATE_IDLE;
    m_instances[index].block_id        = 0;
    m_instances[index].dst_tid         = m_instances[index].dst_port;
    m_instances[index].retries         = 0;
    m_instances[index].request_timeout = 0;
    memcpy(&m_instances[index].connect_params,
           &m_instances[index].init_params,
           sizeof(iot_tftp_trans_params_t));
}


/**@brief Generates event for application.
 *
 * @param[in] index  Index of TFTP instance.
 * @param[in] evt_id  Index of TFTP instance.
 * @param[in] err_code  Index of TFTP instance.
 * @param[in] p_msg  Index of TFTP instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static void handle_evt(uint32_t index, iot_tftp_evt_id_t evt_id, uint32_t err_code, char * p_msg)
{
    uint32_t       internal_err;
    iot_tftp_evt_t evt;

    evt.id           = evt_id;
    evt.p_file = m_instances[index].p_file;

    if (evt_id == IOT_TFTP_EVT_ERROR)
    {
        evt.param.err.code            = err_code;
        evt.param.err.p_msg           = p_msg;
        evt.param.err.size_transfered = m_instances[index].block_id * m_instances[index].connect_params.block_size;

        TFTP_TRC("[TFTP] Raise an ERROR event.\r\n");

        if ((err_code & IOT_TFTP_ERR_BASE) == IOT_TFTP_ERR_BASE)
        {
            evt.param.err.code = CONVERT_TO_TFTP_ERROR(err_code);

            internal_err = send_err_msg(index, &evt.param.err);
            if(internal_err != NRF_SUCCESS)
            {
                TFTP_TRC("[TFTP] Cannot send error message to peer %08lx.\r\n", internal_err);
            }
        }

        // Restore error code.
        evt.param.err.code = err_code;

        // Return to IDLE and notify.
        instance_abort(index);

        app_notify(&index, &evt);
    }
    else if ((evt_id == IOT_TFTP_EVT_TRANSFER_GET_COMPLETE) ||
             (evt_id == IOT_TFTP_EVT_TRANSFER_PUT_COMPLETE))
    {
        evt.id = evt_id;

        TFTP_TRC("[TFTP]: Raise TRANSFER COMPLETE event.\r\n");

        if (m_instances[index].state == STATE_RECV_HOLD)
        {
            TFTP_TRC("[TFTP]: Holding last ACK transfer.\r\n");
            m_instances[index].state = STATE_RECV_COMPLETE;
        }
        else if (m_instances[index].state != STATE_RECV_COMPLETE)
        {
            // Skip into IDLE state.
            instance_abort(index);

            app_notify(&index, &evt);
        }
    }
}

/**@brief: Find instance number by Transmission ID (UDP source port).
 *
 * @param[in]  port     UDP port number on which new message was received.
 * @param[out] p_index  Index of found TFTP instance assigned to the passed UDP port.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static __INLINE uint32_t find_instance_by_tid(uint16_t port, uint32_t * p_index)
{
    uint32_t index;

    for (index = 0; index < TFTP_MAX_INSTANCES; index++)
    {
        if (m_instances[index].src_tid == port)
        {
            break;
        }
    }

    if (index == TFTP_MAX_INSTANCES)
    {
        return (NRF_ERROR_NOT_FOUND | IOT_TFTP_ERR_BASE);
    }

    *p_index = index;

    return NRF_SUCCESS;
}

/**@brief: Negotiation procedure. This function skips through input option string and modifies instance parameters according to negotiated values.
 *
 * @param[in]  p_iter      Pointer to iterator configured to parse RQ/OACK packet.
 * @param[out] p_instance  Pointer to the instance, which parameters should be negotiated.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else TFTP_OPTION_REJECT error indicating
 *                     that server/client requests unsupported option values.
 */
static uint32_t option_negotiate(option_iter_t * p_iter, tftp_instance_t * p_instance)
{
    uint32_t err_code;
    bool     op_size_set    = false;
    bool     op_blksize_set = false;
    bool     op_time_set    = false;

    TFTP_TRC("[TFTP]: Negotiate options: \r\n");

    if (p_iter != NULL) // If NULL passed - reset option values to those defined by RFC.
    {
        UNUSED_VARIABLE(op_get_next(p_iter));

        while (p_iter->curr.p_key != p_iter->p_end)
        {
            if (strcmp_ci(p_iter->curr.p_key, OPTION_TIMEOUT) == 0)
            {
                if (p_instance->init_params.next_retr != 0)
                {
                    uint16_t server_time = str_to_uint(p_iter->curr.p_value);

                    if (server_time < p_instance->init_params.next_retr) // Take minimum.
                    {
                        p_instance->connect_params.next_retr = server_time;
                    }
                    else
                    {
                        p_instance->connect_params.next_retr = p_instance->init_params.next_retr;
                    }

                    op_time_set = true;

                    TFTP_TRC("[TFTP]:    TIMEOUT: %ld\r\n", p_instance->connect_params.next_retr);
                }
            }
            else if (strcmp_ci(p_iter->curr.p_key, OPTION_SIZE) == 0)
            {
                uint32_t file_size = str_to_uint(p_iter->curr.p_value);

                err_code = iot_file_fopen(p_instance->p_file, file_size);
                if (err_code != NRF_SUCCESS)
                {
                    TFTP_TRC("[TFTP]:    TSIZE:   REJECT!\r\n");
                    return TFTP_OPTION_REJECT;
                }

                op_size_set = true;
                TFTP_TRC("[TFTP]:    TSIZE:   %ld\r\n", file_size);
            }
            else if (strcmp_ci(p_iter->curr.p_key, OPTION_BLKSIZE) == 0)
            {
                uint16_t block_size = str_to_uint(p_iter->curr.p_value);
                op_blksize_set = true;

                if (p_instance->init_params.block_size >= block_size)
                {
                    p_instance->connect_params.block_size = block_size;
                }
                else
                {
                    TFTP_TRC("[TFTP]:    BLKSIZE: REJECT!\r\n");
                    return TFTP_OPTION_REJECT;
                }

                TFTP_TRC("[TFTP]:    BLKSIZE: %d\r\n", p_instance->connect_params.block_size);
            }
            else if ((strlen(p_iter->curr.p_key) > 0) && (p_iter->curr.p_value == p_iter->p_end))
            {
                // Password option.
                TFTP_TRC("[TFTP]:    PASSWORD FOUND: %s\r\n", p_iter->curr.p_key);
                p_iter->curr.p_key = p_iter->p_end;
            }
            else
            {
                TFTP_TRC("[TFTP]:    UNKNOWN OPTION\r\n");
            }

            UNUSED_VARIABLE(op_get_next(p_iter));
        }
    }

    // Set values of not negotiated options.
    if (!op_size_set)
    {
        err_code = iot_file_fopen(p_instance->p_file, 0);// Open with default file size (not known).

        if (err_code != NRF_SUCCESS)
        {
            TFTP_TRC("[TFTP]:    TSIZE:   REJECT!\r\n");
            return TFTP_OPTION_REJECT;
        }

        TFTP_TRC("[TFTP]:    TSIZE:   %ld\r\n", p_instance->p_file->file_size);
    }

    if (!op_blksize_set)
    {
        if ((p_instance->init_params.block_size < TFTP_DEFAULT_BLOCK_SIZE) &&
            (p_instance->init_params.block_size != 0))
        {
            TFTP_TRC("[TFTP]:    BLKSIZE: REJECT!\r\n");
            return TFTP_OPTION_REJECT;
        }
        else
        {
            p_instance->connect_params.block_size = TFTP_DEFAULT_BLOCK_SIZE;
        }

        TFTP_TRC("[TFTP]:    BLKSIZE: %d\r\n", p_instance->connect_params.block_size);
    }

    if (!op_time_set)
    {
        p_instance->connect_params.next_retr = p_instance->init_params.next_retr;

        TFTP_TRC("[TFTP]:    TIMEOUT: %ld\r\n", p_instance->connect_params.next_retr);
    }

    return NRF_SUCCESS;
}



/**@brief This function holds ongoing transmissionof TFTP.
 *
 * @param[in] index  Index of TFTP instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t transfer_hold(uint32_t index)
{
    if (m_instances[index].state == STATE_SENDING)
    {
        m_instances[index].state = STATE_SEND_HOLD;
    }
    else if (m_instances[index].state == STATE_RECEIVING)
    {
        m_instances[index].state = STATE_RECV_HOLD;
    }
    else if (m_instances[index].state == STATE_RECV_COMPLETE)
    {
        m_instances[index].state = STATE_RECV_COMPLETE;
    }
    else
    {
        TFTP_ERR("[TFTP]: Hold called in invalid state.\r\n");

        return (NRF_ERROR_INVALID_STATE | IOT_TFTP_ERR_BASE);
    }

    return NRF_SUCCESS;
}


/**@brief This function resumes ongoing transmissionof TFTP.
 *
 * @param[in] index  Index of TFTP instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t transfer_resume(uint32_t index)
{
    uint32_t err_code = NRF_SUCCESS;

    if ((m_instances[index].state != STATE_SEND_HOLD) &&
        (m_instances[index].state != STATE_RECV_HOLD) &&
        (m_instances[index].state != STATE_RECV_COMPLETE))
    {
        TFTP_ERR("[TFTP]: Failed due to invalid state.\r\n");
        TFTP_TRC("[TFTP]: << iot_tftp_resume\r\n");
        return (NRF_ERROR_INVALID_STATE | IOT_TFTP_ERR_BASE);
    }

    err_code = send_response(&index);
    if (err_code != NRF_SUCCESS)
    {
        TFTP_ERR("[TFTP]: Failed to send packet after resume.\r\n");
    }

    if (m_instances[index].state == STATE_SEND_HOLD)
    {
        m_instances[index].state = STATE_SENDING;
    }
    else if (m_instances[index].state == STATE_RECV_HOLD)
    {
        m_instances[index].state = STATE_RECEIVING;
    }
    else if (m_instances[index].state == STATE_RECV_COMPLETE)
    {
        m_instances[index].state = STATE_RECEIVING;
        TFTP_ERR("[TFTP]: Complete due to STATE_RECV_COMPLETE state.\r\n");
        handle_evt(index, IOT_TFTP_EVT_TRANSFER_GET_COMPLETE, err_code, NULL);
    }

    return err_code;
}


/**@brief This function creates ACK packet for specified TFTP instance.
 *
 * @param[in] index     Index of TFTP instance.
 * @param[in] block_id  Data block ID to be acknowledged.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t create_ack_packet(uint32_t index, uint16_t block_id)
{
    uint32_t        err_code;

    TFTP_TRC("[TFTP]: Create ACK packet.\r\n");

    // Reuse p_packet pointer for a new (response) packet. Previous one will be automatically freed by IPv6 module.
    err_code = compose_packet(TYPE_ACK,
                              block_id,
                              &m_instances[index].p_packet,
                              0);

    return err_code;
}

/**@brief This function creates data packet for specified TFTP instance.
 *
 * @param[in] index     Index of TFTP instance.
 * @param[in] block_id  Data block ID to be sent.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t create_data_packet(uint32_t index, uint16_t block_id)
{
    uint32_t         err_code;
    uint32_t         internal_err;
    uint32_t         cursor;
    uint32_t         byte_index;
    iot_pbuffer_t  * p_buffer  = NULL;

    TFTP_TRC("[TFTP]: Create DATA packet.\r\n");

    // If ACK block ID doesn't match last sent packet number.
    if (m_instances[index].block_id != block_id)
    {
        // fseek on file to move to the right point. If fseek returns error - whole transmission will be dropped.
        err_code = iot_file_fseek(m_instances[index].p_file,
                                  (block_id) * m_instances[index].connect_params.block_size);
        if (err_code != NRF_SUCCESS)
        {
            TFTP_ERR("[TFTP]: Unable to fseek on input data file!\r\n");
        }
        else
        {
            m_instances[index].block_id = block_id + 1;
        }
    }
    else
    {
        m_instances[index].block_id = block_id + 1;
    }

    cursor = (m_instances[index].block_id - 1) * m_instances[index].connect_params.block_size;

    // If previous DATA packet wasn't fully filled.
    if (cursor > m_instances[index].p_file->file_size)
    {
        TFTP_TRC("[TFTP]: Transfer complete. Don't send data.\r\n");
        handle_evt(index, IOT_TFTP_EVT_TRANSFER_PUT_COMPLETE, NRF_SUCCESS, NULL);
        return NRF_SUCCESS;
    }
    else if (cursor + m_instances[index].connect_params.block_size >
             m_instances[index].p_file->file_size) // If current sendto operation will send all remaining data.
    {
        TFTP_TRC("[TFTP]: Send last data packet.\r\n");
        err_code = compose_packet(TYPE_DATA,
                                  m_instances[index].block_id,
                                  &p_buffer,
                                  m_instances[index].p_file->file_size - cursor);
    }
    else
    {
        TFTP_TRC("[TFTP]: Send regular data packet.\r\n");
        err_code = compose_packet(TYPE_DATA,
                                  m_instances[index].block_id,
                                  &p_buffer,
                                  m_instances[index].connect_params.block_size);
    }

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    TFTP_TRC("[TFTP]: Created packet:\r\n");
    TFTP_TRC("[TFTP]:     length: %ld\r\n", p_buffer->length);
    TFTP_TRC("[TFTP]:     type:   %d\r\n", p_buffer->p_payload[0] * 256 + p_buffer->p_payload[1]);
    TFTP_TRC("[TFTP]:     ID:     %d\r\n", p_buffer->p_payload[2] * 256 + p_buffer->p_payload[3]);

    byte_index  = TFTP_HEADER_SIZE + TFTP_BLOCK_ID_SIZE;

    // Save reference to correctly filled packet buffer.
    m_instances[index].p_packet = p_buffer;

    if (p_buffer->length - TFTP_HEADER_SIZE - TFTP_BLOCK_ID_SIZE != 0)
    {
        TFTP_MUTEX_UNLOCK();

        // Hold transfer.
        internal_err = transfer_hold(index);
        if (internal_err != NRF_SUCCESS)
        {
            TFTP_ERR("[TFTP]: Error while holding the transfer. Reason: %08lx.\r\n", internal_err);
        }

        err_code = iot_file_fread(m_instances[index].p_file,
                                  &(p_buffer->p_payload[byte_index]),
                                  p_buffer->length - TFTP_HEADER_SIZE - TFTP_BLOCK_ID_SIZE);

        // Unlock instance if file has not assigned callback (probably no needs more time to perform read/write).
        if (m_instances[index].p_file->p_callback == NULL)
        {
            internal_err = transfer_resume(index);
            if (internal_err != NRF_SUCCESS)
            {
                TFTP_ERR("[TFTP]: Error while resuming the transfer. Reason: %08lx.\r\n", internal_err);
            }
        }
        TFTP_MUTEX_LOCK();
    }
    else
    {
        // TFTP is going to send empty data packet, so file callback won't be called.
        internal_err = send_response(&index);
        if (internal_err != NRF_SUCCESS)
        {
            TFTP_ERR("[TFTP]: Error while sending response. Reason: %08lx.\r\n", internal_err);
        }
    }

    if (err_code != NRF_SUCCESS)
    {
        TFTP_ERR("[TFTP]: Failed to save received data (fread)!\r\n");
        handle_evt(index, IOT_TFTP_EVT_ERROR, TFTP_ACCESS_DENIED, ACCESS_ERROR_MSG);
    }

    return err_code;
}


/**@brief Callback handler to receive data on the UDP port.
 *
 * @param[in]   p_socket         Socket identifier.
 * @param[in]   p_ip_header      IPv6 header containing source and destination addresses.
 * @param[in]   p_udp_header     UDP header identifying local and remote endpoints.
 * @param[in]   process_result   Result of data reception, there could be possible errors like
 *                               invalid checksum etc.
 * @param[in]   p_rx_packet      Packet buffer containing the received data packet.
 *
 * @retval NRF_SUCCESS          Indicates received data was handled successfully, else an an
 *                              error code indicating reason for failure..
 */
static uint32_t client_process(const udp6_socket_t * p_socket,
                               const ipv6_header_t * p_ip_header,
                               const udp6_header_t * p_udp_header,
                               uint32_t              process_result,
                               iot_pbuffer_t       * p_rx_packet)
{
    uint32_t         index;
    iot_tftp_evt_t   evt;
    uint8_t        * p_new_packet;
    uint32_t         byte_index;
    uint32_t         err_code;
    uint32_t         internal_err;
    uint16_t         packet_opcode;
    option_iter_t    oack_iter;
    uint16_t         recv_block_id;

    TFTP_TRC("[TFTP]: >> client_process\r\n");

    TFTP_MUTEX_LOCK();

    err_code = find_instance_by_tid(p_udp_header->destport, &index);
    if (err_code != NRF_SUCCESS)
    {
        TFTP_ERR("[TFTP]: Unable to find TFTP instance associated with given UDP port.\r\n");

        // Send TID error to the server.
        err_code = send_err_tid(p_socket, &p_ip_header->srcaddr, p_udp_header->destport);

        TFTP_TRC("[TFTP]: << client_process\r\n");
        TFTP_MUTEX_UNLOCK();

        return err_code;
    }

    // Check UDP status code.
    if (process_result != NRF_SUCCESS)
    {
        TFTP_ERR("[TFTP]: UDP error.\r\n");

        evt.id              = IOT_TFTP_EVT_ERROR;
        evt.param.err.code  = process_result;
        evt.param.err.p_msg = UDP_ERROR_MSG;

        // Call user callback (inform about error).
        app_notify(&index, &evt);

        TFTP_TRC("[TFTP]: << client_process\r\n");

        TFTP_MUTEX_UNLOCK();

        return process_result;
    }

    // Check packet length.
    if (p_rx_packet->length < TFTP_HEADER_SIZE + TFTP_BLOCK_ID_SIZE)
    {
        TFTP_ERR("[TFTP]: Invalid packet length.\r\n");

        evt.id              = IOT_TFTP_EVT_ERROR;
        evt.param.err.code  = TFTP_INVALID_PACKET;
        evt.param.err.p_msg = (char *)"Invalid packet length!";

        app_notify(&index, &evt);

        TFTP_TRC("[TFTP]: << client_process\r\n");

        TFTP_MUTEX_UNLOCK();

        return evt.param.err.code;
    }

    // Read received packet type.
    p_new_packet  = p_rx_packet->p_payload;
    packet_opcode = uint16_decode(p_new_packet);
    packet_opcode = NTOHS(packet_opcode);
    byte_index    = TFTP_HEADER_SIZE;

    if ((m_instances[index].state == STATE_SEND_HOLD) ||
        (m_instances[index].state == STATE_RECV_HOLD) ||
        (m_instances[index].state == STATE_IDLE))
    {
        TFTP_TRC("[TFTP]: Ignore packets in HOLD/IDLE states.\r\n");

        TFTP_MUTEX_UNLOCK();

        TFTP_TRC("[TFTP]: << client_process\r\n");

        return NRF_SUCCESS;  // Ignore retransmission of other side.
    }

    switch (packet_opcode)
    {
        case TYPE_OACK:
            if ((m_instances[index].state != STATE_CONNECTING_RRQ) &&
                (m_instances[index].state != STATE_CONNECTING_WRQ) &&
                (m_instances[index].state != STATE_IDLE && m_instances[index].retries > 0))
            {
                TFTP_ERR("[TFTP]: Invalid TFTP instance state! \r\n");

                break;
            }

            op_init(&oack_iter,
                    (char *)&p_new_packet[byte_index],
                    p_rx_packet->length - TFTP_HEADER_SIZE); // Options uses whole packet except opcode.

            TFTP_TRC("[TFTP]: Received OACK.\r\n");
            err_code = option_negotiate(&oack_iter, &m_instances[index]);
            if (err_code != NRF_SUCCESS)
            {
                TFTP_ERR("[TFTP]: Failed to negotiate options!.\r\n");
                handle_evt(index, IOT_TFTP_EVT_ERROR, TFTP_OPTION_REJECT, OPTION_ERROR_MESSAGE);

                break;
            }

            // Set server transmission id.
            m_instances[index].dst_tid  = p_udp_header->srcport;

            if (m_instances[index].state == STATE_CONNECTING_RRQ)
            {
                m_instances[index].p_packet = p_rx_packet;
                m_instances[index].state    = STATE_RECEIVING;

                err_code = create_ack_packet(index, 0);

                if(err_code == NRF_SUCCESS)
                {
                    err_code = send_response(&index);
                }
            }
            else if (m_instances[index].state == STATE_CONNECTING_WRQ)
            {
                m_instances[index].state = STATE_SENDING;

                err_code = create_data_packet(index, 0);
            }
            else
            {
                TFTP_ERR("[TFTP]: Incorrect state to receive OACK!.\r\n");
            }

            if (err_code != NRF_SUCCESS)
            {
                TFTP_ERR("[TFTP]: Failed to create packet!.\r\n");
                handle_evt(index, IOT_TFTP_EVT_ERROR, err_code, NULL);
            }
            break;

        case TYPE_ACK:
            recv_block_id = uint16_decode(&p_new_packet[byte_index]);
            recv_block_id = NTOHS(recv_block_id);

            if (m_instances[index].state == STATE_CONNECTING_WRQ)
            {
                TFTP_TRC("[TFTP]: Options not supported. Received ACK.\r\n");
                err_code = option_negotiate(NULL, &m_instances[index]);
                if (err_code != NRF_SUCCESS)
                {
                    TFTP_ERR("[TFTP]: Failed to negotiate default options!.\r\n");
                    handle_evt(index, IOT_TFTP_EVT_ERROR, TFTP_OPTION_REJECT, OPTION_ERROR_MESSAGE);

                    break;
                }

                // Set server transmission id.
                m_instances[index].dst_tid  = p_udp_header->srcport;

                // Set instance state.
                m_instances[index].state    = STATE_SENDING;
                m_instances[index].block_id = 0;
            }

            if (m_instances[index].state == STATE_SENDING || m_instances[index].state == STATE_RECEIVING)
            {
                if (recv_block_id == m_instances[index].block_id)
                {
                    m_instances[index].retries = 0;
                }
                TFTP_TRC("[TFTP]: Received ACK. Send block %4d of %ld.\r\n", m_instances[index].block_id + 1, 
                    CEIL_DIV(m_instances[index].p_file->file_size, m_instances[index].connect_params.block_size) +
                    ((m_instances[index].p_file->file_size % m_instances[index].connect_params.block_size == 0) ? 0 : 1));

                err_code = create_data_packet(index, recv_block_id);
                if ((err_code != (NRF_ERROR_DATA_SIZE | IOT_TFTP_ERR_BASE)) && (err_code != NRF_SUCCESS))
                {
                    TFTP_ERR("[TFTP]: Failed to create data packet.\r\n");
                    handle_evt(index, IOT_TFTP_EVT_ERROR, err_code, NULL);
                }
            }
            else
            {
                TFTP_ERR("[TFTP]: Invalid state to receive ACK packet.\r\n");

                TFTP_MUTEX_UNLOCK();

                TFTP_ERR("[TFTP]: << client_process\r\n");

                return NRF_SUCCESS;
            }
            break;

        case TYPE_DATA:
            recv_block_id  = uint16_decode(&p_new_packet[byte_index]);
            recv_block_id  = NTOHS(recv_block_id);
            byte_index    += TFTP_BLOCK_ID_SIZE;

            if (m_instances[index].state == STATE_CONNECTING_RRQ)
            {
                TFTP_TRC("[TFTP]: Options not supported. Received DATA.\r\n");
                err_code = option_negotiate(NULL, &m_instances[index]);
                if (err_code != NRF_SUCCESS)
                {
                    TFTP_ERR("[TFTP]: Failed to set default options.\r\n");
                    handle_evt(index, IOT_TFTP_EVT_ERROR, TFTP_OPTION_REJECT, OPTION_ERROR_MESSAGE);

                    break;
                }

                if (recv_block_id == 1) // Received first data block.
                {
                    m_instances[index].block_id = 0;
                    m_instances[index].state = STATE_RECEIVING;
                }

                // Set server transmission id.
                m_instances[index].dst_tid = p_udp_header->srcport;
            }

            if (m_instances[index].state == STATE_RECEIVING)
            {
                TFTP_TRC("[TFTP]: Received DATA.\r\n");

                m_instances[index].p_packet = p_rx_packet;

                if (recv_block_id == m_instances[index].block_id + 1)
                {
                    TFTP_TRC("[TFTP]: Received next DATA (n+1).\r\n");

                    m_instances[index].retries = 0;

                    err_code = create_ack_packet(index, m_instances[index].block_id + 1);

                    if(err_code != NRF_SUCCESS)
                    {
                        TFTP_ERR("[TFTP]: Failed to create ACK packet.\r\n");
                        handle_evt(index, IOT_TFTP_EVT_ERROR, err_code, NULL);
                        break;
                    }
                }
                else
                {
                    TFTP_TRC("[TFTP]: Skip current DATA packet. Try to request proper block ID by sending ACK.\r\n");

                    err_code = create_ack_packet(index, m_instances[index].block_id);

                    if(err_code == NRF_SUCCESS)
                    {
                        err_code = send_response(&index);
                    }
                    else
                    {
                        TFTP_ERR("[TFTP]: Failed to create ACK packet.\r\n");
                        handle_evt(index, IOT_TFTP_EVT_ERROR, err_code, NULL);
                    }
                }

                // Check if payload size is smaller than defined block size.
                if ((p_rx_packet->length - TFTP_BLOCK_ID_SIZE - TFTP_HEADER_SIZE) < 
                    m_instances[index].connect_params.block_size)
                {
                    m_instances[index].state = STATE_RECV_COMPLETE;
                }
                else if (err_code != NRF_SUCCESS)
                {
                    TFTP_ERR("[TFTP]: Failed to create ACK packet.\r\n");
                    handle_evt(index, IOT_TFTP_EVT_ERROR, err_code, NULL);

                    break;
                }

                TFTP_TRC("[TFTP]: Send block %4d of %ld ACK.\r\n", m_instances[index].block_id, 
                    m_instances[index].p_file->file_size / m_instances[index].connect_params.block_size);

                if (recv_block_id == m_instances[index].block_id + 1)
                {
                    m_instances[index].block_id = recv_block_id;
                    TFTP_MUTEX_UNLOCK();

                    if (p_rx_packet->length - byte_index > 0)
                    {
                        internal_err = transfer_hold(index);
                        if (internal_err != NRF_SUCCESS)
                        {
                            TFTP_ERR("[TFTP]: Error while holding the transfer. Reason: %08lx.\r\n", internal_err);
                        }

                        err_code = iot_file_fwrite(m_instances[index].p_file,
                                                   &p_new_packet[byte_index],
                                                   p_rx_packet->length - byte_index);

                        // Unlock instance if file has not assigned callback (probably not needs more time to perform read/write).
                        if (m_instances[index].p_file->p_callback == NULL)
                        {
                            internal_err = transfer_resume(index);
                            if (internal_err != NRF_SUCCESS)
                            {
                                TFTP_ERR("[TFTP]: Error while resuming the transfer. Reason: %08lx.\r\n", internal_err);
                            }
                        }
                    }
                    else
                    {
                        err_code = iot_file_fclose(m_instances[index].p_file);

                        internal_err = send_response(&index);
                        if (internal_err != NRF_SUCCESS)
                        {
                            TFTP_ERR("[TFTP]: Error while sending response. Reason: %08lx.\r\n", internal_err);
                        }

                        TFTP_ERR("[TFTP]: Complete due to packet length. (%ld: %ld)\r\n", p_rx_packet->length, byte_index);
                        m_instances[index].state = STATE_RECEIVING;
                        handle_evt(index, IOT_TFTP_EVT_TRANSFER_GET_COMPLETE, NRF_SUCCESS, NULL);
                    }

                    TFTP_MUTEX_LOCK();

                    if (err_code != NRF_SUCCESS)
                    {
                        TFTP_ERR("[TFTP]: Failed to save received data (fwrite)!\r\n");
                        handle_evt(index, IOT_TFTP_EVT_ERROR, TFTP_ACCESS_DENIED, ACCESS_ERROR_MSG);

                        break;
                    }
                }
            }
            else
            {
                TFTP_ERR("[TFTP]: << Invalid state to receive DATA packet.\r\n");

                TFTP_MUTEX_UNLOCK();

                TFTP_ERR("[TFTP]: << client_process\r\n");

                return NRF_SUCCESS;
            }
            break;

        case TYPE_ERR:
            recv_block_id  = uint16_decode(&p_new_packet[byte_index]);
            recv_block_id  = NTOHS(recv_block_id);
            byte_index    += TFTP_ERR_CODE_SIZE;

            TFTP_ERR("[TFTP]: Received error packet!\r\n");

            evt.id              = IOT_TFTP_EVT_ERROR;
            evt.param.err.code  = CONVERT_TO_IOT_ERROR(recv_block_id);

            if (p_rx_packet->length > TFTP_HEADER_SIZE + TFTP_ERR_CODE_SIZE)
            {
                evt.param.err.p_msg                 = (char *) &p_new_packet[byte_index];
                p_new_packet[p_rx_packet->length-1] = '\0';
            }

            app_notify(&index, &evt);

            TFTP_MUTEX_UNLOCK();

            TFTP_TRC("[TFTP]: << client_process\r\n");

            if (evt.param.err.code != TFTP_INVALID_TID)
            {
                instance_abort(index);
            }

            return evt.param.err.code;

        default:
            TFTP_ERR("[TFTP]: Invalid TFTP packet opcode! \r\n");
            handle_evt(index, IOT_TFTP_EVT_ERROR, TFTP_INVALID_PACKET, NULL);

            break;
    }

    TFTP_TRC("[TFTP]: << client_process\r\n");

    return err_code;
}

/**@brief Count how many bytes are required to store options inside request packet.
 *
 * @param[in] index  Index of TFTP instance.
 * @param[in] type   Type of TFTP request.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t count_options_length(uint32_t index, uint32_t type)
{
    uint32_t op_length = 0;
    char     next_retr_str[NEXT_RETR_MAX_LENGTH];
    char     block_size_str[BLKSIZE_MAX_LENGTH];
    char     file_size_str[FILE_SIZE_MAX_LENGTH];

    if ((m_instances[index].init_params.next_retr > 0) &&
        (m_instances[index].init_params.next_retr < 256))
    {
        UNUSED_VARIABLE(uint_to_str(m_instances[index].init_params.next_retr, next_retr_str, NEXT_RETR_MAX_LENGTH));
        op_length += sizeof(OPTION_TIMEOUT);         // Time out option length.
        op_length += strlen(next_retr_str) + 1;      // The '\0' character ate the end of a string.
    }

    if ((m_instances[index].init_params.block_size > 0) &&
        (m_instances[index].init_params.block_size != TFTP_DEFAULT_BLOCK_SIZE))
    {
        UNUSED_VARIABLE(uint_to_str(m_instances[index].init_params.block_size, block_size_str, sizeof(block_size_str)));
        op_length += sizeof(OPTION_BLKSIZE);         // Time out option length.
        op_length += strlen(block_size_str) + 1;     // The '\0' character ate the end of a string.
    }

    op_length += sizeof(OPTION_SIZE);                // TFTP tsize option length.

    if (type == TYPE_RRQ)
    {
        op_length += sizeof(OPTION_SIZE_REQUEST_VALUE); // Just send 0 to inform other side that you support this option and would like to receive file size inside OptionACK.
    }

    if (type == TYPE_WRQ)
    {
        UNUSED_VARIABLE(uint_to_str(m_instances[index].p_file->file_size, file_size_str, sizeof(file_size_str)));
        op_length += strlen(file_size_str) + 1;
    }

    if (m_instances[index].p_password != NULL)
    {
        if (m_instances[index].p_password[0] != '\0')
        {
            op_length += strlen(m_instances[index].p_password) + 1; // Password is always sent as last string without option name.
        }
    }

    return op_length;
}

/**@brief This function inserts options into request packet payload using passed option iterator.
 *
 * @param[in] index  Index of TFTP instance.
 * @param[in] type   Type of TFTP request.
 * @param[in] p_iter Iterator which will be used to set options.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t insert_options(uint32_t index, uint32_t type, option_iter_t * p_iter)
{
    uint32_t err_code = NRF_SUCCESS;
    char     next_retr_str[NEXT_RETR_MAX_LENGTH];
    char     block_size_str[BLKSIZE_MAX_LENGTH];
    char     file_size_str[FILE_SIZE_MAX_LENGTH];

    if (type == TYPE_RRQ)
    {
        err_code = op_set(p_iter, OPTION_SIZE, OPTION_SIZE_REQUEST_VALUE);
    }

    if (type == TYPE_WRQ)
    {
        UNUSED_VARIABLE(uint_to_str(m_instances[index].p_file->file_size, file_size_str, FILE_SIZE_MAX_LENGTH));
        err_code = op_set(p_iter, OPTION_SIZE, file_size_str);
    }

    if (err_code == NRF_SUCCESS)
    {
        if ((m_instances[index].init_params.next_retr > 0) &&
            (m_instances[index].init_params.next_retr < 256))
        {
            UNUSED_VARIABLE(uint_to_str(m_instances[index].init_params.next_retr, next_retr_str, NEXT_RETR_MAX_LENGTH));
            err_code = op_set(p_iter, OPTION_TIMEOUT, next_retr_str);
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }
        }

        if ((m_instances[index].init_params.block_size > 0) &&
            (m_instances[index].init_params.block_size != TFTP_DEFAULT_BLOCK_SIZE))
        {
            UNUSED_VARIABLE(uint_to_str(m_instances[index].init_params.block_size, block_size_str, BLKSIZE_MAX_LENGTH));
            err_code = op_set(p_iter, OPTION_BLKSIZE, block_size_str);
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }
        }

        if (m_instances[index].p_password != NULL)
        {
            if (m_instances[index].p_password[0] != '\0')
            {
                err_code = op_set(p_iter, NULL, m_instances[index].p_password);
            }
        }
    }

    return err_code;
}

/**@Sends Read/Write TFTP Request.
 *
 * @param[in]  type      Type of request (allowed values: TYPE_RRQ and TYPE_WRQ).
 * @param[in]  p_tftp    Pointer to the TFTP instance (from user space).
 * @param[in]  p_file    Pointer to the file, which should be assigned to passed instance.
 * @param[in]  p_params  Pointer to transmission parameters structure (retransmission time, block size).
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
static uint32_t send_request(uint32_t     type,
                             iot_tftp_t * p_tftp,
                             iot_file_t * p_file)
{
    uint32_t                    err_code;
    iot_pbuffer_t             * p_buffer;
    iot_pbuffer_alloc_param_t   buffer_param;
    uint8_t                   * p_rrq_packet;
    option_iter_t               rrq_iter;
    uint32_t                    index;
    uint32_t                    byte_index;

    err_code = find_instance(p_tftp, &index);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if (p_file->p_filename[0] == '\0')
    {
        TFTP_ERR("[TFTP]: Invalid file name passed.\r\n");
        return (NRF_ERROR_INVALID_PARAM | IOT_TFTP_ERR_BASE);
    }

    if (m_instances[index].state != STATE_IDLE)
    {
        TFTP_ERR("[TFTP]: Invalid instance state.\r\n");
        return (NRF_ERROR_INVALID_STATE | IOT_TFTP_ERR_BASE);
    }

    // Assign file with TFTP instance.
    m_instances[index].p_file                 = p_file;
    m_instances[index].block_id               = 0;
    m_instances[index].dst_tid                = m_instances[index].dst_port;

    memset(&buffer_param, 0, sizeof(buffer_param));
    buffer_param.type  = UDP6_PACKET_TYPE;
    buffer_param.flags = PBUFFER_FLAG_DEFAULT;

    // Calculate size of required packet.
    buffer_param.length  = TFTP_HEADER_SIZE;                   // Bytes reserved for TFTP opcode value.
    buffer_param.length += strlen(p_file->p_filename) + 1;     // File name with '\0' character.
    buffer_param.length += sizeof(OPTION_MODE_OCTET);          // Mode option value length.

    TFTP_TRC("[TFTP]: Estimated packet length without options: %ld.\r\n", buffer_param.length);

    buffer_param.length += count_options_length(index, type);  // TFTP options.

    TFTP_TRC("[TFTP]: Estimated packet length with options: %ld.\r\n", buffer_param.length);

    // Allocate packet buffer.
    err_code = iot_pbuffer_allocate(&buffer_param, &p_buffer);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    memset(p_buffer->p_payload, 0, buffer_param.length);
    byte_index = 0;

    // Compose TFTP Read Request according to configuration.
    p_rrq_packet  = p_buffer->p_payload;
    uint16_t size = uint16_encode(HTONS(type), &p_rrq_packet[byte_index]);
    byte_index   += size;

    // Initialization of option iterator.
    op_init(&rrq_iter, (char *)&p_rrq_packet[byte_index], buffer_param.length - TFTP_HEADER_SIZE); // Options uses whole packet except opcode.
    rrq_iter.p_start[0] = '\0';

    // Insert file name and mode strings.
    err_code = op_set(&rrq_iter, NULL, p_file->p_filename);
    PBUFFER_FREE_IF_ERROR(err_code);

    err_code = op_set(&rrq_iter, NULL, OPTION_MODE_OCTET);
    PBUFFER_FREE_IF_ERROR(err_code);

    // Insert TFTP options into packet.
    err_code = insert_options(index, type, &rrq_iter);
    PBUFFER_FREE_IF_ERROR(err_code);

    // Change instance status to connecting.
    if (type == TYPE_RRQ)
    {
        m_instances[index].state = STATE_CONNECTING_RRQ;
    }
    else
    {
        m_instances[index].state = STATE_CONNECTING_WRQ;
    }

    // Send read request.
    UNUSED_VARIABLE(retr_timer_reset(index));
    err_code = udp6_socket_sendto(&m_instances[index].socket,
                                  &m_instances[index].addr,
                                  m_instances[index].dst_tid,
                                  p_buffer);

    if (err_code != NRF_SUCCESS)
    {
        TFTP_ERR("[TFTP]: Unable to send request.\r\n");
        m_instances[index].state = STATE_IDLE;
    }

    PBUFFER_FREE_IF_ERROR(err_code);

    return NRF_SUCCESS;
}

/**@brief Function used in order to change initial connection parameters. */
uint32_t iot_tftp_set_params(iot_tftp_t * p_tftp, iot_tftp_trans_params_t * p_params)
{
    uint32_t err_code;
    uint32_t index;

    NULL_PARAM_CHECK(p_params);
    NULL_PARAM_CHECK(p_tftp);

    TFTP_TRC("[TFTP]: >> iot_tftp_set_params\r\n");

    TFTP_MUTEX_LOCK();

    err_code = find_instance(p_tftp, &index);
    if (err_code == NRF_SUCCESS)
    {
        // Modifying connection parameters could be done only when TFTP instance is disconnected.
        // NOTE: STATE_FREE is not allowed, because there have to be a moment (e.g. after calling iot_tftp_init()) when transmission parameters were set to default values.
        if (m_instances[index].state == STATE_IDLE)
        {
            // Reset connection parameters to initial values. They will be set (negotiated) after get/put call.
            memcpy(&m_instances[index].init_params, p_params, sizeof(iot_tftp_trans_params_t));
        }
        else
        {
            err_code = (NRF_ERROR_INVALID_STATE | IOT_TFTP_ERR_BASE);

            TFTP_ERR("[TFTP]: Cannot modify connection parameters inside %d state.\r\n", m_instances[index].state);
        }
    }
    else
    {
        TFTP_ERR("[TFTP]: Unable to find TFTP instance.\r\n");
    }

    TFTP_MUTEX_UNLOCK();

    TFTP_TRC("[TFTP]: << iot_tftp_set_params\r\n");

    return err_code;
}

/**@brief Function for performing retransmissions of TFTP acknowledgements. */
void iot_tftp_timeout_process(iot_timer_time_in_ms_t wall_clock_value)
{
    uint32_t index;
    uint32_t err_code;

    UNUSED_PARAMETER(wall_clock_value);

    TFTP_MUTEX_LOCK();

    for (index = 0; index < TFTP_MAX_INSTANCES; index++)
    {
        if ((m_instances[index].state == STATE_CONNECTING_RRQ) ||
            (m_instances[index].state == STATE_CONNECTING_WRQ) ||
            (m_instances[index].state == STATE_SENDING)        ||
            (m_instances[index].state == STATE_RECEIVING))
        {
            TFTP_TRC("[TFTP]: >> tftp_timeout_process\r\n");
            TFTP_TRC("[TFTP]: Current timer: %ld, %ld\r\n", m_instances[index].request_timeout, wall_clock_value);

            if (instance_timer_is_expired(index))
            {
                err_code = NRF_SUCCESS;

                if (m_instances[index].retries < TFTP_MAX_RETRANSMISSION_COUNT)
                {
                    TFTP_TRC("[TFTP]: Query retransmission [%d] for file %s.\r\n",
                             m_instances[index].retries, m_instances[index].p_file->p_filename);

                    // Increase retransmission number.
                    m_instances[index].retries++;

                    TFTP_TRC("[TFTP]: Compose packet for retransmission.\r\n");
                    // Send packet again.
                    if (m_instances[index].state == STATE_RECEIVING)
                    {
                        TFTP_TRC("[TFTP]:     Retransmission of ACK packet.\r\n");
                        err_code = create_ack_packet(index, m_instances[index].block_id);

                        if (err_code == NRF_SUCCESS)
                        {
                            err_code = send_response(&index);
                        }
                        else
                        {
                            TFTP_ERR("[TFTP]: Failed to create packet!.\r\n");
                            handle_evt(index, IOT_TFTP_EVT_ERROR, err_code, NULL);
                        }
                    }
                    else if (m_instances[index].state == STATE_SENDING)
                    {
                        TFTP_TRC("[TFTP]:     Retransmission of DATA packet.\r\n");
                        err_code = create_data_packet(index, m_instances[index].block_id - 1);

                        if (err_code == NRF_SUCCESS)
                        {
                            if (m_instances[index].p_file->p_callback == NULL)
                            {
                                err_code = send_response(&index);
                            }
                        }
                        else
                        {
                            TFTP_ERR("[TFTP]: Failed to create packet!.\r\n");
                            handle_evt(index, IOT_TFTP_EVT_ERROR, err_code, NULL);
                        }
                    }
                    else if (m_instances[index].state == STATE_CONNECTING_RRQ)
                    {
                        TFTP_TRC("[TFTP]:     OACK time out. Retransmit RRQ.\r\n");
                        m_instances[index].state = STATE_IDLE;
                        err_code = send_request(TYPE_RRQ, &index, m_instances[index].p_file);
                    }
                    else if (m_instances[index].state == STATE_CONNECTING_WRQ)
                    {
                        TFTP_TRC("[TFTP]:     OACK time out. Retransmit WRQ.\r\n");
                        m_instances[index].state = STATE_IDLE;
                        err_code = send_request(TYPE_WRQ, &index, m_instances[index].p_file);
                    }
                    else
                    {
                        TFTP_TRC("[TFTP]: In idle state!\r\n");
                    }
                }
                else
                {
                    TFTP_ERR("[TFTP]: TFTP server did not response on query for file %s.\r\n",
                             m_instances[index].p_file->p_filename);

                    // No response from server.
                    err_code = TFTP_REMOTE_UNREACHABLE;
                }

                if (err_code != NRF_SUCCESS)
                {
                    // Inform application that timeout occurs.
                    TFTP_ERR("[TFTP]: Timeout error.\r\n");
                    handle_evt(index, IOT_TFTP_EVT_ERROR, err_code, NULL);

                }
            }
            TFTP_MUTEX_UNLOCK();

            TFTP_TRC("[TFTP]: << tftp_timeout_process\r\n");

            return;
        }
    }

    TFTP_MUTEX_UNLOCK();
}

/**@brief Initializes TFTP client. */
uint32_t iot_tftp_init(iot_tftp_t * p_tftp, iot_tftp_init_t * p_init_params)
{
    uint32_t index = 0;
    uint32_t err_code;

    NULL_PARAM_CHECK(p_tftp);
    NULL_PARAM_CHECK(p_init_params);
    NULL_PARAM_CHECK(p_init_params->p_ipv6_addr);

    TFTP_TRC("[TFTP]: >> iot_tftp_init\r\n");

    TFTP_MUTEX_LOCK();

    // Find first available instance.
    err_code = find_free_instance(&index);
    if (err_code == NRF_SUCCESS)
    {
        // Reset instance values.
        instance_reset(index);

        // Assign new values.
        *p_tftp = index;
        m_instances[index].callback   = p_init_params->callback;
        m_instances[index].src_tid    = p_init_params->src_port;
        m_instances[index].dst_port   = p_init_params->dst_port;
        m_instances[index].p_password = p_init_params->p_password;
        m_instances[index].dst_tid    = m_instances[index].dst_port;
        memcpy(&m_instances[index].addr, p_init_params->p_ipv6_addr, sizeof(ipv6_addr_t));

        // Configure socket.
        err_code = udp6_socket_allocate(&m_instances[index].socket);
        if (err_code == NRF_SUCCESS)
        {
            err_code = udp6_socket_bind(&m_instances[index].socket,
                                        IPV6_ADDR_ANY,
                                        p_init_params->src_port);
            if (err_code == NRF_SUCCESS)
            {
                // Attach callback.
                err_code = udp6_socket_recv(&m_instances[index].socket, client_process);
                if (err_code == NRF_SUCCESS)
                {
                    m_instances[index].state = STATE_IDLE;
                }
            }

            if (err_code != NRF_SUCCESS)
            {
                (void)udp6_socket_free(&m_instances[index].socket);

                TFTP_ERR("[TFTP]: UDP socket configuration failure.\r\n");
            }
        }
    }
    else
    {
        TFTP_ERR("[TFTP]: No more free instances left.\r\n");
    }

    TFTP_MUTEX_UNLOCK();

    TFTP_TRC("[TFTP]: << iot_tftp_init\r\n");

    return err_code;
}

/**@brief Resets TFTP client instance, so it is possible to make another request after error. */
uint32_t iot_tftp_abort(iot_tftp_t * p_tftp)
{
    uint32_t err_code;
    uint32_t index;

    NULL_PARAM_CHECK(p_tftp);

    TFTP_TRC("[TFTP]: >> iot_tftp_abort\r\n");

    TFTP_MUTEX_LOCK();

    err_code = find_instance(p_tftp, &index);
    if (err_code == NRF_SUCCESS)
    {
        instance_abort(index);
    }

    TFTP_MUTEX_UNLOCK();

    TFTP_TRC("[TFTP]: << iot_tftp_abort\r\n");

    return err_code;
}

/**@brief Frees assigned sockets. */
uint32_t iot_tftp_uninit(iot_tftp_t * p_tftp)
{
    uint32_t       err_code;
    uint32_t       index;

    NULL_PARAM_CHECK(p_tftp);

    TFTP_TRC("[TFTP]: >> iot_tftp_uninit\r\n");

    TFTP_MUTEX_LOCK();

    err_code = find_instance(p_tftp, &index);
    if (err_code == NRF_SUCCESS)
    {
        if (m_instances[index].state == STATE_SEND_HOLD ||
            m_instances[index].state == STATE_RECV_HOLD ||
            m_instances[index].state == STATE_SENDING   ||
            m_instances[index].state == STATE_RECEIVING)
        {
            // Free pbuffer.
            err_code = iot_pbuffer_free(m_instances[index].p_packet, true);
            if (err_code != NRF_SUCCESS)
            {
                TFTP_ERR("[TFTP]: Cannot free pbuffer - %p\r\n", m_instances[index].p_packet);
            }

            err_code = iot_file_fclose(m_instances[index].p_file);
            if (err_code != NRF_SUCCESS)
            {
                TFTP_ERR("[TFTP]: Cannot close file - %p\r\n", m_instances[index].p_file);
            }
        }

        if(m_instances[index].state != STATE_IDLE)
        {
            handle_evt(index, IOT_TFTP_EVT_ERROR, TFTP_UNDEFINED_ERROR, UNINT_ERROR_MSG);
        }

        (void)udp6_socket_free(&m_instances[index].socket);
        m_instances[index].state = STATE_FREE;
    }

    TFTP_MUTEX_UNLOCK();

    TFTP_TRC("[TFTP]: << iot_tftp_uninit\r\n");

    return err_code;
}

/**@brief Retrieves file from remote server into p_file. */
uint32_t iot_tftp_get(iot_tftp_t * p_tftp, iot_file_t * p_file)
{
    uint32_t err_code;

    NULL_PARAM_CHECK(p_tftp);
    NULL_PARAM_CHECK(p_file);
    NULL_PARAM_CHECK(p_file->p_filename);

    TFTP_TRC("[TFTP]: >> iot_tftp_get\r\n");

    TFTP_MUTEX_LOCK();

    err_code = send_request(TYPE_RRQ, p_tftp, p_file);
    if (err_code != NRF_SUCCESS)
    {
        TFTP_ERR("[TFTP]: Error while sending read request. Reason: %08lx.\r\n", err_code);
    }

    TFTP_MUTEX_UNLOCK();

    TFTP_TRC("[TFTP]: << iot_tftp_get\r\n");

    return err_code;
}

/**@brief Sends local file p_file to a remote server. */
uint32_t iot_tftp_put(iot_tftp_t * p_tftp, iot_file_t * p_file)
{
    uint32_t err_code;

    NULL_PARAM_CHECK(p_tftp);
    NULL_PARAM_CHECK(p_file);
    NULL_PARAM_CHECK(p_file->p_filename);

    TFTP_TRC("[TFTP]: >> iot_tftp_put\r\n");

    TFTP_MUTEX_LOCK();

    err_code = send_request(TYPE_WRQ, p_tftp, p_file);
    if (err_code != NRF_SUCCESS)
    {
        TFTP_ERR("[TFTP]: Error while sending write request. Reason: %08lx.\r\n", err_code);
    }

    TFTP_MUTEX_UNLOCK();

    TFTP_TRC("[TFTP]: << iot_tftp_put\r\n");

    return err_code;
}

/**@brief Holds transmission of ACK (use in order to slow transmission). */
uint32_t iot_tftp_hold(iot_tftp_t * p_tftp)
{
    uint32_t index;
    uint32_t err_code;

    NULL_PARAM_CHECK(p_tftp);

    TFTP_TRC("[TFTP]: >> iot_tftp_hold\r\n");

    TFTP_MUTEX_LOCK();

    err_code = find_instance(p_tftp, &index);
    if (err_code == NRF_SUCCESS)
    {
        // Hold transfer.
        err_code = transfer_hold(index);
    }
    else
    {
        TFTP_ERR("[TFTP]: Hold called on unknown TFTP instance.\r\n");
    }

    TFTP_MUTEX_UNLOCK();

    TFTP_TRC("[TFTP]: << iot_tftp_hold\r\n");

    return err_code;
}

/**@brief Resumes transmission. */
uint32_t iot_tftp_resume(iot_tftp_t * p_tftp)
{
    uint32_t index;
    uint32_t err_code;

    NULL_PARAM_CHECK(p_tftp);

    TFTP_TRC("[TFTP]: >> iot_tftp_resume\r\n");

    TFTP_MUTEX_LOCK();

    err_code = find_instance(p_tftp, &index);
    if (err_code == NRF_SUCCESS)
    {
        err_code = transfer_resume(index);
    }
    else
    {
        TFTP_ERR("[TFTP]: Failed to find instance.\r\n");
    }

    TFTP_MUTEX_UNLOCK();

    TFTP_TRC("[TFTP]: << iot_tftp_resume\r\n");

    return err_code;
}
