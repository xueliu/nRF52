/* Copyright (c)  2014 Nordic Semiconductor. All Rights Reserved.
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

/** @file iot_common.h
 *
 * @defgroup iot_common Common utils
 * @ingroup iot_sdk_common
 * @{
 * @brief Common IoT macros that are needed by IoT modules.
 *
 * @details This module abstracts common macros related to IoT. These macros can be used by all IoT modules.
 */

#ifndef IOT_COMMON_H__
#define IOT_COMMON_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble_gap.h"
#include "iot_defines.h"
#include "iot_errors.h"
#include "sdk_os.h"

/**@brief Context identifiers used in stateful compression. */
typedef struct
{
    uint8_t src_cntxt_id;                                                                           /**< Source context identifier (if any). IPV6_CONTEXT_IDENTIFIER_NONE by default. IP Stack can set this on the interface if needed. */
    uint8_t dest_cntxt_id;                                                                          /**< Destination context identifier (if any). IPV6_CONTEXT_IDENTIFIER_NONE by default. IP Stack can set this on the interface if needed. */
}iot_context_id_t;

/**@brief Context structure used for efficient compression and decompression. */
typedef struct
{
    uint8_t      context_id;                                                                        /**< Context identifier (CID) number. */
    uint8_t      prefix_len;                                                                        /**< Length of prefix for CID. */
    ipv6_addr_t  prefix;                                                                            /**< Prefix for CID. */
    bool         compression_flag;                                                                  /**< Indicates if the context can be used for compression. */
}iot_context_t;

/**@brief Encapsulates all information of the 6LoWPAN interface. */
typedef struct
{
    eui64_t                 local_addr;                                                             /**< Local EUI-64 address. */
    eui64_t                 peer_addr;                                                              /**< Peer EUI-64 address. */
    iot_context_id_t        tx_contexts;                                                            /**< TX contexts can be used for effective compression. */
    void                  * p_upper_stack;                                                          /**< Block to upper stack. */
    void                  * p_transport;                                                            /**< Transport reference. */
}iot_interface_t;

#endif //IOT_COMMON_H__

/**@} */
