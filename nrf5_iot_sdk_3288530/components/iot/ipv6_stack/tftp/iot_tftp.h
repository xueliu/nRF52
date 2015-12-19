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

/** @file iot_tftp.h
 *
 * @defgroup iot_tftp TFTP Application Interface for Nordic's IPv6 stack
 * @ingroup iot_sdk_stack
 * @{
 * @brief Trivial File Transfer Protocol module provides implementation of TFTP Client.
 *
 */

#ifndef IOT_TFTP_H__
#define IOT_TFTP_H__

#include "sdk_common.h"
#include "iot_common.h"
#include "iot_timer.h"
#include "iot_file.h"
#include "iot_defines.h"

/**@brief TFTP global instance number. */
typedef uint32_t iot_tftp_t;

/**@brief TFTP module Events. */
typedef enum
{
    IOT_TFTP_EVT_ERROR,                                                                             /**< Event code indicating that en error occurred. */
    IOT_TFTP_EVT_TRANSFER_GET_COMPLETE,                                                             /**< Event code indicating that transfer read was completed. */
    IOT_TFTP_EVT_TRANSFER_PUT_COMPLETE,                                                             /**< Event code indicating that transfer write was completed. */
} iot_tftp_evt_id_t;

/**@brief TFTP error event structure. */
typedef struct
{
    uint32_t   code;                                                                                /**< Error code. */
    char     * p_msg;                                                                               /**< Message describing the reason or NULL if no description is available. */
    uint32_t   size_transfered;                                                                     /**< In case of an error, variable indicates a number of successfully read or write bytes. */
} iot_tftp_evt_err_t;

/**@brief TFTP event structure. */
typedef struct
{
    iot_tftp_evt_err_t err;                                                                         /**< Error event structure. Used only in case of IOT_TFTP_EVT_ERROR error. */
} iot_tftp_evt_param_t;

/**@brief Asynchronous event type. */
typedef struct
{
    iot_tftp_evt_id_t      id;                                                                      /**< Event code. */
    iot_tftp_evt_param_t   param;                                                                   /**< Union to structures describing event. */
    iot_file_t           * p_file;                                                                  /**< File associated with TFTP transfer. */
} iot_tftp_evt_t;

/**@brief TFTP Transmission initialization structure (both GET and PUT). */
typedef struct
{
    uint32_t next_retr;                                                                             /**< Number of seconds between retransmissions. */
    uint16_t block_size;                                                                            /**< Maximum or negotiated size of data block. */
} iot_tftp_trans_params_t;

/**@brief User callback from TFTP module.
 *
 * @note TFTP module user callback will be invoked even if user asks TFTP to abort (TFTP error event).
 *
 * @param[in] p_tftp  Pointer to the TFTP instance.
 * @param[in] p_evt   Pointer to the TFTP event structure, describing reason.
 *
 * @retval None.
 */
typedef void (*iot_tftp_callback_t)(iot_tftp_t * p_tftp, iot_tftp_evt_t * p_evt);


/**@brief TFTP initialization structure. */
typedef struct
{
    ipv6_addr_t         * p_ipv6_addr;                                                              /**< IPv6 address of the server. */
    uint16_t              src_port;                                                                 /**< Source port (local UDP port) from which all request and data will be sent. Should be choosen randomly. */
    uint16_t              dst_port;                                                                 /**< Destination port - UDP port on which server listens for new connections. */
    iot_tftp_callback_t   callback;                                                                 /**< Reference to the user callback. */
    const char          * p_password;                                                               /**< Server password for all requests. Shall be NULL if no password is required. */
} iot_tftp_init_t;


/**@brief Initializes TFTP client.
 *
 * @param[in] p_tftp         Pointer to the TFTP instance. Should not be NULL.
 * @param[in] p_init_params  Initialization structure for TFTP client. Should not be NULL.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_tftp_init(iot_tftp_t * p_tftp, iot_tftp_init_t * p_init_params);


/**@brief Function used in order to change initial connection parameters.
 *
 * @param[in]  p_tftp    Reference to the TFTP instance.
 * @param[in]  p_params  Pointer to transmission parameters structure. Should not be NULL.
 *
 * @retval NRF_SUCCESS if parameters successfully set, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_tftp_set_params(iot_tftp_t * p_tftp, iot_tftp_trans_params_t * p_params);


/**@brief Retrieves file from remote server into p_file.
 *
 * @param[in] p_tftp  Pointer to the TFTP instance.
 * @param[in] p_file  Reference to the file from which data should be read. Should not be NULL.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_tftp_get(iot_tftp_t * p_tftp, iot_file_t * p_file);


/**@brief Sends local file p_file to a remote server.
 *
 * @param[in] p_tftp  Pointer to the TFTP instance.
 * @param[in] p_file  Reference to the file to which data should be stored. Should not be NULL.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_tftp_put(iot_tftp_t * p_tftp, iot_file_t * p_file);


/**@brief Holds transmission of ACK (use in order to slow transmission).
 *
 * @param[in] p_tftp  Pointer to the TFTP instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_tftp_hold(iot_tftp_t * p_tftp);


/**@brief Resumes transmission.
 *
 * @param[in] p_tftp  Pointer to the TFTP instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_tftp_resume(iot_tftp_t * p_tftp);


/**@brief Resets TFTP client instance, so it is possible to make another request after error. 
 *
 * @param[in] p_tftp  Pointer to the TFTP instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_tftp_abort(iot_tftp_t * p_tftp);


/**@brief Frees assigned sockets.
 *
 * @param[in] p_tftp  Pointer to the TFTP instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_tftp_uninit(iot_tftp_t * p_tftp);


/**@brief Function for performing retransmissions of TFTP acknowledgements.
 *
 * @note TFTP module implements the retransmission mechanism by invoking this function periodically.
 *       So that method has to be added to IoT Timer client list and has to be called with minimum of
 *       TFTP_RETRANSMISSION_INTERVAL resolution.
 *
 * @param[in] wall_clock_value  The value of the wall clock that triggered the callback.
 *
 * @retval None.
 */
void iot_tftp_timeout_process(iot_timer_time_in_ms_t wall_clock_value);

#endif

/** @} */
