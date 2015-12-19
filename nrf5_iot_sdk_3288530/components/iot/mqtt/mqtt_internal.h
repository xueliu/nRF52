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

/** @file mqtt_internal.h
 *
 * @brief Function and data strcutures internal to MQTT module.
 */

#ifndef MQTT_INTERNAL_H_
#define MQTT_INTERNAL_H_

#include "nordic_common.h"
#include "mqtt.h"
#include "iot_errors.h"
#include "nrf_tls.h"
#include "app_trace.h"
#include <stdint.h>
#include <string.h>
#include "nrf_error.h"
#include "nrf_tls.h"


//#define MQTT_TRC app_trace_log
#define MQTT_TRC(...)

#ifndef MQTT_MAX_CLIENTS
#define MQTT_MAX_CLIENTS          1                                            /**< Maximum number of clients that can be managed by the module. */
#endif //MQTT_MAX_CLIENTS

#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE            60                                           /**< Keep alive time for MQTT (in seconds). Sending of Ping Requests to be keep the connection alive are governed by this value. */
#endif //MQTT_KEEPALIVE

#ifndef MQTT_MAX_PACKET_LENGTH
#define MQTT_MAX_PACKET_LENGTH    128                                          /**< Maximum MQTT packet size that can be sent (including the fixed and variable header). */
#endif // MQTT_MAX_PACKET_LENGTH

#define MQTT_FIXED_HEADER_SIZE              2                                  /**< Fixed header minimum size. Remaining length size is 1 in this case. */
#define MQTT_FIXED_HEADER_EXTENDED_SIZE     5                                  /**< Maxium size of the fixed header.  Remaining length size is 4 in this case. */

#define MQTT_VERSION_3_1_0_PROTO_LEVEL      3                                  /**< Protocol level for 3.1.0. */
#define MQTT_VERSION_3_1_1_PROTO_LEVEL      4                                  /**< Protocol level for 3.1.1. */

/**@brief MQTT Control Packet Types. */
#define MQTT_PKT_TYPE_CONNECT      0x10
#define MQTT_PKT_TYPE_CONNACK      0x20
#define MQTT_PKT_TYPE_PUBLISH      0x30
#define MQTT_PKT_TYPE_PUBACK       0x40
#define MQTT_PKT_TYPE_PUBREC       0x50
#define MQTT_PKT_TYPE_PUBREL       0x60
#define MQTT_PKT_TYPE_PUBCOMP      0x70
#define MQTT_PKT_TYPE_SUBSCRIBE    0x82 // QoS 1 for subscribe
#define MQTT_PKT_TYPE_SUBACK       0x90
#define MQTT_PKT_TYPE_UNSUBSCRIBE  0xA2
#define MQTT_PKT_TYPE_UNSUBACK     0xB0
#define MQTT_PKT_TYPE_PINGREQ      0xC0
#define MQTT_PKT_TYPE_PINGRSP      0xD0
#define MQTT_PKT_TYPE_DISCONNECT   0xE0


/**@brief Masks for MQTT header flags. */
#define MQTT_HEADER_DUP_MASK       0x08
#define MQTT_HEADER_QOS_MASK       0x06
#define MQTT_HEADER_RETAIN_MASK    0x01

/**@brief Masks for MQTT header flags. */
#define MQTT_CONNECT_FLAG_CLEAN_SESSION 0x02
#define MQTT_CONNECT_FLAG_WILL_TOPIC    0x04
#define MQTT_CONNECT_FLAG_WILL_RETAIN   0x20
#define MQTT_CONNECT_FLAG_PASSWORD      0x40
#define MQTT_CONNECT_FLAG_USERNAME      0x80

/**@brief Size of mandatory header of MQTT Packet. */
#define MQTT_PKT_HEADER_SIZE       2

/**@brief  Ping packte size. */
#define MQTT_PING_PKT_SIZE         2

/**@brief */
#define MQTT_MAX_PAYLOAD_SIZE      0x0FFFFFFF

/**@brief Maximum size of variable and payload in the packet.  */
#define MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD (MQTT_MAX_PACKET_LENGTH - MQTT_FIXED_HEADER_EXTENDED_SIZE)

/**@brief Defines size of uint8 in bytes. */
#define SIZE_OF_UINT8                1

/**@brief Defines size of uint8 in bytes. */
#define SIZE_OF_UINT16               2

/**@brief Computes total size needed to pack a UTF8 string. */
#define GET_UT8STR_BUFFER_SIZE(STR) (SIZE_OF_UINT16 + (STR)->utf_strlen)

/**@brief Computes total size needed to pack a binary stream. */
#define GET_BINSTR_BUFFER_SIZE(STR) ((STR)->bin_strlen)

/**@brief Unsubscribe packet size. */
#define MQTT_UNSUBSCRIBE_PKT_SIZE  4

/**@brief Sets MQTT Client's state with one indicated in 'STATE'. */
#define MQTT_SET_STATE(CLIENT, STATE)   SET_BIT((CLIENT)->state, (STATE))

/**@brief Verifies if MQTT Client's state is set with one indicated in 'STATE'. */
#define MQTT_VERIFY_STATE(CLIENT, STATE) IS_SET((CLIENT)->state, (STATE))

/**@brief Reset 'STATE' in MQTT Client's state. */
#define MQTT_RESET_STATE(CLIENT, STATE) CLR_BIT((CLIENT)->state, (STATE))

/**@brief Initialize MQTT Client's state. */
#define MQTT_STATE_INIT(CLIENT) (CLIENT)->state = MQTT_STATE_IDLE

/**@brief Computes the first byte of MQTT message header based on messae type, duplication flag,
 *        QoS and  the retain flag.
 */
#define MQTT_MESSAGES_OPTIONS(TYPE, DUP, QOS, RETAIN)    \
        (((TYPE)      & 0xF0)   |                        \
        (((DUP) << 3) & 0x08)   |                        \
        (((QOS) << 1) & 0x06)   |                        \
        ((RETAIN) & 0x01))


/**
 * @defgroup iot_mqtt_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case the need to use an alternative architecture arises.
 * @{
 */
#define MQTT_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_mqtt_mutex)                       /**< Lock module using mutex */
#define MQTT_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_mqtt_mutex)                     /**< Unlock module using mutex */
/** @} */

/**@brief Check if the input pointer is NULL, if so it returns NRF_ERROR_NULL.
 */
#define NULL_PARAM_CHECK(PARAM)                                                                   \
        if ((PARAM) == NULL)                                                                      \
        {                                                                                         \
            return (NRF_ERROR_NULL | IOT_MQTT_ERR_BASE);                                          \
        }

#define NULL_PARAM_CHECK_VOID(PARAM)                                                              \
        if ((PARAM) == NULL)                                                                      \
        {                                                                                         \
            return;                                                                               \
        }

/**@brief MQTT States. */
typedef enum
{
    MQTT_STATE_IDLE           = 0x00000000,                                    /**< Idle state, implying the client entry in the table is unused/free. */
    MQTT_STATE_TCP_CONNECTING = 0x00000001,                                    /**< TCP Connection has been requested, awaiting result of the request. */
    MQTT_STATE_TCP_CONNECTED  = 0x00000002,                                    /**< TCP Connection successfully established. */
    MQTT_STATE_CONNECTED      = 0x00000004,                                    /**< MQTT Connection successful. */
    MQTT_STATE_PENDING_WRITE  = 0x00000008                                     /**< State that indicates write callback is awaited for an issued request. */
} mqtt_state_t;

/**@brief Transport for handling transport connect procedure. */
typedef uint32_t (*transport_connect_handler_t)(mqtt_client_t * p_client);

/**@brief Transport trite handler. */
typedef uint32_t (*transport_write_handler_t)(mqtt_client_t * p_client, uint8_t const * data, uint32_t datalen);

/**@breif Transport read handler. */
typedef uint32_t (*transport_read_handler_t)(mqtt_client_t * p_client, uint8_t * data, uint32_t datalen);

/**@breif Transport disconenct handler. */
typedef uint32_t (*transport_disconnect_handler_t)(mqtt_client_t * p_client);

/**@breif Transport procedure handlers. */
typedef struct
{
    transport_connect_handler_t    connect;                                    /**< Transport connect handler. Handles TCP connection callback based on type of transport.*/
    transport_write_handler_t      write;                                      /**< Transport write handler. Handles transport write based on type of transport. */
    transport_read_handler_t       read;                                       /**< Transport read handler. Handles transport read based on type of transport. */
    transport_disconnect_handler_t disconnect;                                 /**< Transport disconnect handler. Handles transport disconnection based on type of transport. */
} transport_procedure_t;

/**@brief Initiates TCP Connection.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 */
uint32_t tcp_request_connection(mqtt_client_t * p_client);


/**@brief Handles TCP Connection Complete for TCP(non-secure) transport.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 */
uint32_t mqtt_client_tcp_connect(mqtt_client_t * p_client);


/**@brief Handles write requests on TCP(non-secure) transport.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 * @param[in] p_data   Data to be written on the transport.
 * @param[in] datalen  Length of data to be written on the transport.
 */
uint32_t mqtt_client_tcp_write(mqtt_client_t * p_client, uint8_t const * p_data, uint32_t datalen);


/**@brief Handles read requests on TCP(non-secure) transport.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 * @param[in] p_data   Pointer where read data is to be fetched.
 * @param[in] datalen  Size of memory provided for the operation.
 */
uint32_t mqtt_client_tcp_read(mqtt_client_t * p_client, uint8_t * p_data, uint32_t datalen);


/**@brief Handles transport disconnection requests on TCP(non-secure) transport.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 */
uint32_t mqtt_client_tcp_disconnect(mqtt_client_t * p_client);


/**@brief Handles read requests on TLS(secure) transport.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 * @param[in] p_data   Pointer where read data is to be fetched.
 * @param[in] datalen  Size of memory provided for the operation.
 */
uint32_t mqtt_client_tls_connect(mqtt_client_t * p_client);


/**@brief Handles write requests on TLS(secure) transport.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 * @param[in] p_data   Data to be written on the transport.
 * @param[in] datalen  Length of data to be written on the transport.
 */
uint32_t mqtt_client_tls_write(mqtt_client_t * p_client, uint8_t const * p_data, uint32_t datalen);


/**@brief Handles read requests on TLS(secure) transport.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 * @param[in] p_data   Pointer where read data is to be fetched.
 * @param[in] datalen  Size of memory provided for the operation.
 */
uint32_t mqtt_client_tls_read(mqtt_client_t * p_client, uint8_t * p_data, uint32_t datalen);


/**@brief Handles transport disconnection requests on TLS(secure) transport.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 */
uint32_t mqtt_client_tls_disconnect(mqtt_client_t * p_client);


/**@brief Aborts TCP connection.
 *
 * @param[in] p_client Idenitifies the client on which the procedure is requested.
 */
void mqtt_client_tcp_abort(mqtt_client_t * p_client);


/**@brief Packs unsigned 8 bit value to the buffer at the offset requested.
 *
 * @param[in]    val        Value to be packed.
 * @param[in]    buffer_len Total size of the buffer on which value is to be packed.
 *                          This shall not be zero.
 * @param[out]   p_buffer   Buffer where the value is to be packed.
 * @param[inout] p_offset   Offset on the buffer where the value is to be packed. If the procedure
 *                          is successful, the offset is incremented to point to the next write/pack
 *                          location on the buffer.
 *
 * @retval NRF_SUCCESS         if procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length.
 */
uint32_t pack_uint8(uint8_t      val,
                    uint32_t     buffer_len,
                    uint8_t    * const p_buffer,
                    uint32_t   * const p_offset);


/**@brief Packs unsigned 16 bit value to the buffer at the offset requested.
 *
 * @param[in]    val        Value to be packed.
 * @param[in]    buffer_len Total size of the buffer on which value is to be packed.
 *                          This shall not be zero.
 * @param[out]   p_buffer   Buffer where the value is to be packed.
 * @param[inout] p_offset   Offset on the buffer where the value is to be packed. If the procedure
 *                          is successful, the offset is incremented to point to the next write/pack
 *                          location on the buffer.
 *
 * @retval NRF_SUCCESS         if the procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length minus
 *                             the size of unsigned 16 bit interger.
 */
uint32_t pack_uint16(uint16_t     val,
                     uint32_t     buffer_len,
                     uint8_t    * const p_buffer,
                     uint32_t   * const p_offset);


/**@brief Packs utf8 string to the buffer at the offset requested.
 *
 * @param[in]    p_str      UTF-8 string and its length to be packed.
 * @param[in]    buffer_len Total size of the buffer on which string is to be packed.
 *                          This shall not be zero.
 * @param[out]   p_buffer   Buffer where the string is to be packed.
 * @param[inout] p_offset   Offset on the buffer where the string is to be packed. If the procedure
 *                          is successful, the offset is incremented to point to the next write/pack
 *                          location on the buffer.
 *
 * @retval NRF_SUCCESS         if the procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length minus
 *                             the size of unsigned 16 bit interger.
 * @retval NRF_ERROR_NO_MEM    if there is no room on the buffer to pack the string.
 */
uint32_t pack_utf8_str(mqtt_utf8_t const * const p_str,
                       uint32_t                  buffer_len,
                       uint8_t           * const p_buffer,
                       uint32_t          * const p_offset);


/**@brief Packs binary string to the buffer at the offset requested.
 *
 * @param[in]    p_str      Binary string and its length to be packed.
 * @param[in]    buffer_len Total size of the buffer on which string is to be packed.
 * @param[in]    p_buffer   Buffer where the string is to be packed.
 * @param[inout] p_offset   Offset on the buffer where the string is to be packed. If the procedure
 *                          is successful, the offset is incremented to point to the next write/pack
 *                          location on the buffer.
 *
 * @retval NRF_SUCCESS         if the procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length.
 * @retval NRF_ERROR_NO_MEM    if there is no room on the buffer to pack the string.
 */
uint32_t pack_bin_str(mqtt_binstr_t const * const p_str,
                      uint32_t                    buffer_len,
                      uint8_t             * const p_buffer,
                      uint32_t            * const p_offset);


/**@brief Unpacks unsigned 8 bit value from the buffer from the offset requested.
 *
 * @param[out]   p_val      Memory where the value is to be unpacked.
 * @param[in]    buffer_len Total size of the buffer. This shall not be zero.
 * @param[in]    p_buffer   Buffer from which the value is to be unpacked.
 * @param[inout] p_offset   Offset on the buffer from where the value is to be unpacked. If the
 *                          procedure is successful, the offset is incremented to point to the next
 *                          read/unpack location on the buffer.
 *
 * @retval NRF_SUCCESS         if the procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length.
 */
uint32_t unpack_uint8(uint8_t     *       p_val,
                      uint32_t            buffer_len,
                      uint8_t     * const p_buffer,
                      uint32_t    * const p_offset);

/**@brief Unpacks unsigned 16 bit value from the buffer from the offset requested.
 *
 * @param[out]   p_val      Memory where the value is to be unpacked.
 * @param[in]    buffer_len Total size of the buffer. This shall not be zero.
 * @param[in]    p_buffer   Buffer from which the value is to be unpacked.
 * @param[inout] p_offset   Offset on the buffer from where the value is to be unpacked. If the
 *                          procedure is successful, the offset is incremented to point to the next
 *                          read/unpack location on the buffer.
 *
 * @retval NRF_SUCCESS         if the procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length.
 */
uint32_t unpack_uint16(uint16_t    *       p_val,
                       uint32_t            buffer_len,
                       uint8_t     * const p_buffer,
                       uint32_t    * const p_offset);


/**@brief Unpacks unsigned 16 bit value from the buffer from the offset requested.
 *
 * @param[out]   p_str      Memory where theutf8 string and its value are to be unpacked.
 *                          No copy of data is performed for the string.
 * @param[in]    buffer_len Total size of the buffer. This shall not be zero.
 * @param[in]    p_buffer   Buffer from which the string is to be unpacked.
 * @param[inout] p_offset   Offset on the buffer from where the value is to be unpacked. If the
 *                          procedure is successful, the offset is incremented to point to the next
 *                          read/unpack location on the buffer.
 *
 * @retval NRF_SUCCESS         if the procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length.
 */
uint32_t unpack_utf8_str(mqtt_utf8_t    * const p_str,
                         uint32_t               buffer_len,
                         uint8_t        * const p_buffer,
                         uint32_t       * const p_offset);


/**@brief Unpacks binary string from the buffer from the offset requested.
 *
 * @param[out]   p_str      Memory where the binary string and its length are to be unpacked.
 *                          No copy of data is performed for the string.
 * @param[in]    buffer_len Total size of the buffer. This shall not be zero.
 * @param[in]    p_buffer   Buffer where the value is to be unpacked.
 * @param[inout] p_offset   Offset on the buffer from where the value is to be unpacked. If the
 *                          procedure is successful, the offset is incremented to point to the next
 *                          read/unpack location on the buffer.
 *
 * @retval NRF_SUCCESS         if the procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length.
 */
uint32_t unpack_bin_str(mqtt_binstr_t   * const p_str,
                        uint32_t                buffer_len,
                        uint8_t         * const p_buffer,
                        uint32_t        * const p_offset);


/**@brief Unpacks utf8 string from the buffer from the offset requested.
 *
 * @param[out]   p_str      Memory where the utf8 string and its length are to be unpacked.
 * @param[in]    buffer_len Total size of the buffer. This shall not be zero.
 * @param[in]    p_buffer   Buffer where the value is to be unpacked.
 * @param[inout] p_offset   Offset on the buffer from where the value is to be unpacked. If the
 *                          procedure is successful, the offset is incremented to point to the next
 *                          read/unpack location on the buffer.
 *
 * @retval NRF_SUCCESS         if the procedure is successful.
 * @retval NRF_ERROR_DATA_SIZE if the offset is greater than or equal to the buffer length.
 */
uint32_t zero_len_str_encode(uint32_t            buffer_len,
                             uint8_t     * const p_buffer,
                             uint32_t    * const p_offset);


/**@brief Computes and encodes length for the MQTT fixed header.
 *
 * @note The remaining length is not packed as a fixed unsigned 32 bit integer. Instead it is packed
 *       on algorithm below:
 *
 * @code
 * do
 *            encodedByte = X MOD 128
 *            X = X DIV 128
 *            // if there are more data to encode, set the top bit of this byte
 *            if ( X > 0 )
 *                encodedByte = encodedByte OR 128
 *            endif
 *                'output' encodedByte
 *       while ( X > 0 )
 * @endcode
 *
 * @param[in]    remaining_length Length of variable header and payload in the MQTT message.
 * @param[out]   p_buffer         Buffer where the length is to be packed.
 * @param[inout] p_offset         Offset on the buffer where the length is to be packed.
 */
void packet_length_encode(uint32_t remaining_length, uint8_t * p_buffer, uint32_t * p_offset);


/**@brief Decode MQTT Packet Length in the MQTT fixed header.
 *
 * @param[out]   p_buffer           Buffer where the length is to be decoded.
 * @param[in]    p_remaining_length Length of variable header and payload in the MQTT message.
 * @param[out]   p_remaining_length Buffer where the length is to be decoded.
 * @param[inout] p_offset           Offset on the buffer from where the length is to be unpacked.
 */
void packet_length_decode(uint8_t * p_buffer, uint32_t * p_remaining_length, uint32_t * p_size);


/**@brief Encodes fixed header for the MQTT message and provides pointer to start of the header.
 *
 * @param[in]    message_type     Message type containing packet type and the flags.
 *                                Use @ref MQTT_MESSAGES_OPTIONS to contruct the message_type.
 * @param[in]    length           Buffer where the message payload along with variable header.
 * @param[inout] pp_packet        Pointer to the MQTT message vraible header and payload.
 *                                The 5 bytes before the start of the message are assumed by the
 *                                routine to be available to pack the fixed header. However, since
 *                                the fixed header length is variable length, the pointer to the
 *                                start of the MQTT message along with encoded fixed header is
 *                                supplied as output parameter if the procedure was successful.
 *
 * @retval 0xFFFFFFFF if the procedure failed, else length of total MQTT message along with the
 *                    fixed header.
 */
uint32_t mqtt_encode_fixed_header(uint8_t message_type, uint32_t length, uint8_t ** pp_packet);


/**@brief Handles MQTT messages received from the peer. For TLS, this routine is evoked to handle
 *        decrypted application data. For TCP, this routine is evoked to handle TCP data.
 *
 * @param[in]    p_client     Identifies the client for which the data was received.
 * @param[in]    p_data       MQTT data received.
 * @param[inout] datalen      Length of data received.
 *
 * @retval NRF_SUCCESS if the procedure is successul, else an error code indicating the reason
 *                     for failure.
 */
uint32_t mqtt_handle_rx_data(mqtt_client_t * p_client, uint8_t * p_data, uint32_t datalen);


/**@brief Constructs/encodes connect packet.
 *
 * @param[in]  p_client          Identifies the client for which the procedure is requested.
 *                               All information required for creating the packet like client id,
 *                               clean session flag, retain session flag etc are assumed to be
 *                               populated for the client instance when this procedure is requested.
 * @param[out] pp_packet         Pointer to the MQTT connect message.
 * @param[out] p_packet_length   Length of the connect request.
 *
 * @retval 0xFFFFFFFF if the procedure failed, else length of total MQTT message along with the
 *                    fixed header.
 */
void connect_request_encode(const mqtt_client_t *  p_client,
                            uint8_t             ** pp_packet,
                            uint32_t            *  p_packet_length);

/**@biref Function pointer array for selection of correct handler based on transport type. */
extern const transport_procedure_t transport_fn[MQTT_TRASNPORT_MAX];

#endif // MQTT_INTERNAL_H_
