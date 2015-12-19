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

/** @file coap.h
 *
 * @defgroup iot_sdk_coap_api CoAP interface
 * @ingroup iot_sdk_coap
 * @{
 * @brief Interface for the CoAP protocol.
 */

#ifndef COAP_H__
#define COAP_H__

#include "coap_api.h"
#include "app_trace.h"

/**
 * @defgroup iot_coap_log Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests. These are intended for debugging purposes and
 *          can be enabled by defining the COAP_ENABLE_LOGS to 1.
 * @note That if ENABLE_DEBUG_LOG_SUPPORT is disabled, having COAP_ENABLE_LOGS has no effect.
 * @{
 */
#if (COAP_ENABLE_LOGS == 1)

#define COAP_TRC  app_trace_log                                                          /**< Used for getting trace of execution in the module. */
#define COAP_DUMP app_trace_dump                                                         /**< Used for dumping octet information to get details of bond information etc. */
#define COAP_ERR  app_trace_log                                                          /**< Used for logging errors in the module. */

#else // COAP_ENABLE_LOGS

#define COAP_TRC(...)                                                                    /**< Diasbles traces. */
#define COAP_DUMP(...)                                                                   /**< Diasbles dumping of octet streams. */
#define COAP_ERR(...)                                                                    /**< Diasbles error logs. */

#endif // COAP_ENABLE_LOGS

#if (COAP_DISABLE_API_PARAM_CHECK == 0)

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL | IOT_COAP_ERR_BASE);                                               \
    }

/**@brief Verify that parameter members has been set by the application. */
#define NULL_PARAM_MEMBER_CHECK(PARAM)                                                             \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_INVALID_PARAM | IOT_COAP_ERR_BASE);                                      \
    }    
#else 
    
#define NULL_PARAM_CHECK(PARAM)
#define NULL_PARAM_MEMBER_CHECK(PARAM)
    
#endif // COAP_DISABLE_API_PARAM_CHECK

/**
 * @defgroup iot_coap_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case the need to use an alternative architecture arises.
 * @{
 */
#define COAP_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_coap_mutex)                                 /**< Lock module using mutex */
#define COAP_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_coap_mutex)                               /**< Unlock module using mutex */
/** @} */

/**@brief Sends a CoAP message.
 *
 * @details Sends out a request using the underlying transport layer. Before sending, the
 *          \ref coap_message_t structure is serialized and added to an internal message queue
 *          in the library.  The handle returned can be used to abort the message from being 
 *          retransmitted at any time. 
 *
 * @param[out] p_handle  Handle to the message if CoAP CON/ACK messages has been used. Returned
 *                       by reference.
 * @param[in]  p_message Message to be sent.
 * 
 * @retval NRF_SUCCESS If the message was successfully encoded and scheduled for transmission.
 */
uint32_t internal_coap_message_send(uint32_t * p_handle, coap_message_t * p_message);  
    
#endif // COAP_H__

/** @} */
