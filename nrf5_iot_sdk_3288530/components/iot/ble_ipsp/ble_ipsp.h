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
 * @defgroup ble_sdk_ipsp Internet Protocol Support Profile
 * @{
 * @ingroup iot_sdk_6lowpan
 * @brief Internet Protocol Support Profile.
 *
 * @details This module implements the Internet Protocol Support Profile creating and managing
 *          transport for 6lowpan.
 *          GATT is used to discover if IPSP is supported or not, but no IP data is exchanged
 *          over GATT. To exchange data, LE L2CAP Credit Mode is used. The PSM used for the channel
 *          is BLE_IPSP_PSM and is defined by the specification. The MTU mandated by the
 *          specification is 1280 bytes.
 *
 * @note Attention!
 *  To maintain compliance with Nordic Semiconductor ASA's Bluetooth profile
 *  qualification listings, this section of source code must not be modified.
 */

#ifndef BLE_IPSP_H__
#define BLE_IPSP_H__

#include <stdint.h>
#include "ble.h"

/**@brief Maximum IPSP channels required to be supported. */
#define BLE_IPSP_MAX_CHANNELS                              1

/**@brief Maximum Transmit Unit on IPSP channel. */
#define BLE_IPSP_MTU                                       1280

/**@brief Receive MPS used by IPSP. */
#define BLE_IPSP_RX_MPS                                    50

/**@brief Maximum data size that can be received.
 *
 * @details Maximum data size that can be received on the IPSP channel.
 *          Modify this values to intentionally set a receive size less
 *          than the MTU set on the channel.
 */
#define BLE_IPSP_RX_BUFFER_SIZE                            1280

/**@brief Maximum number of receive buffers.
 *
 * @details Maximum number of receive buffers to be used per IPSP channel.
 *          Each receive buffer is of size @ref BLE_IPSP_RX_BUFFER_SIZE. This configuration
 *          has implications on the number of SDUs that can be received
 *          while an SDU is being consumed by the application (6LoWPAN/IP Stack).
 */
#define BLE_IPSP_RX_BUFFER_COUNT                           4

/**@brief L2CAP Protocol Service Multiplexers number. */
#define BLE_IPSP_PSM                                       0x0023

/**@brief Invalid channel ID representation. */
#define BLE_IPSP_INVALID_CID                               0x0000

/**@brief IPSP Event IDs. */
typedef enum
{
    BLE_IPSP_EVT_CHANNEL_CONNECTED,                                                                 /**< Channel connection event. */
    BLE_IPSP_EVT_CHANNEL_DISCONNECTED,                                                              /**< Channel disconnection event. */
    BLE_IPSP_EVT_CHANNEL_DATA_RX,                                                                   /**< Data received on channel event. */
    BLE_IPSP_EVT_CHANNEL_DATA_TX_COMPLETE                                                           /**< Requested data transmission complete on channel event. */
} ble_ipsp_evt_type_t;


/**@brief IPSP event and associated parameter type. */
typedef struct
{
    ble_ipsp_evt_type_t     evt_id;                                                                 /**< Identifier event type. */
    ble_l2cap_evt_t       * evt_param;                                                              /**< Parameters associated with the event. */
    uint32_t                evt_result;                                                             /**< Result of event. If the event result is SDK_ERR_RX_PKT_TRUNCATED for @ref BLE_IPSP_EVT_CHANNEL_DATA_RX, this means that an incomplete SDU was received due to insufficient RX buffer size determined by @ref BLE_IPSP_RX_BUFFER_SIZE. */
} ble_ipsp_evt_t;


/**@brief IPSP handle. */
typedef struct
{
    uint16_t conn_handle;                                                                           /**< Identifies the link on which the IPSP channel is established. */
    uint16_t cid;                                                                                   /**< Identifies the IPSP logical channel. */
} ble_ipsp_handle_t;


/**@brief Profile event handler type.
 *
 * @param[in] p_handle Identifies the connection and channel on which the event occurred.
 * @param[in] p_evt    Event and related parameters (if any).
 *
 * @returns    Provision for the application to indicate if the event was successfully processed or
 *             not. Currently not used.
 */
typedef uint32_t (*ble_ipsp_evt_handler_t) (ble_ipsp_handle_t * p_handle, ble_ipsp_evt_t * p_evt);


/**@brief IPSP initialization structure.
 *
 * @details IPSP initialization structure containing all options and data needed to
 *          initialize the profile.
 */
typedef struct
{
    ble_ipsp_evt_handler_t    evt_handler;                                                          /**< Event notification callback registered with the module to receive asynchronous events. */
} ble_ipsp_init_t;


/**@brief Function for initializing the Internet Protocol Support Profile.
 *
 * @param[in]   p_init  Information needed to initialize the service.
 *
 * @retval  NRF_SUCCESS  If initialization of the service was successful, else, an error code
  *                         indicating reason for failure.
 */
uint32_t ble_ipsp_init(const ble_ipsp_init_t * p_init);


/**@brief Function for requesting a channel creation for the Internet Protocol Support Profile.
 *
 * @details Channel creation for Internet Protocol Support Profile (IPSP) is requested using this
 *          API. Connection handle provided in p_handle parameter identifies the peer with which
 *          the IPSP channel is being requested.
 *          NRF_SUCCESS return value by the API is only indicative of request procedure having
 *          succeeded. Result of channel establishment is known when the
 *          @ref BLE_IPSP_EVT_CHANNEL_CONNECTED event is notified.
 *          Therefore, the application must wait for @ref BLE_IPSP_EVT_CHANNEL_CONNECTED event on
 *          successful return of this API.
 *
 * @param[in]  p_handle  Indicates the connection handle on which IPSP channel is to be created.
 *
 * @retval  NRF_SUCCESS  If channel creation request was successful, else, an error code
  *                         indicating reason for failure.
 */
uint32_t ble_ipsp_connect(const ble_ipsp_handle_t * p_handle);


/**@brief Function for sending IP data to peer.
 *
 * @param[in]   p_handle  Instance of the logical channel and peer for which the data is intended.
 * @param[in]   p_data    Pointer to memory containing the data to be transmitted.
 *                        @note This memory must be resident and should not be freed unless
 *                        @ref BLE_IPSP_EVT_CHANNEL_DATA_TX_COMPLETE event is notified.
 * @param[in]   data_len  Length/size of data to be transferred.
 *
 * @retval      NRF_SUCCESS If sending data was successful, else, an error code
  *                         indicating reason for failure.
 */
uint32_t ble_ipsp_send(ble_ipsp_handle_t const * p_handle,
                       uint8_t           const * p_data,
                       uint16_t                  data_len);


/**@brief Function for disconnecting IP transport.
 *
 * @param[in]   p_handle  Identifies IPSP transport.
 *
 * @retval      NRF_SUCCESS If disconnecting was successful, else, an error code
  *                         indicating reason for failure.
 */
uint32_t ble_ipsp_disconnect(ble_ipsp_handle_t const * p_handle);

/**@brief BLE event handler function.
 *
 * @param[in]   p_evt  BLE event to be handled.
 *
 * @retval      NRF_SUCCESS If handling the event was successful, else, an error code
  *                         indicating reason for failure.
 */
void ble_ipsp_evt_handler(ble_evt_t * p_evt);

#endif // BLE_IPSP_H__

/** @} */
