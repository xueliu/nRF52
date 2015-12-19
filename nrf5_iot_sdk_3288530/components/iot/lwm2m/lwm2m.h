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
 
/** @file lwm2m.h
 *
 * @defgroup iot_sdk_lwm2m_api LWM2M library private definitions.
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief LWM2M library private definititions.
 */
 
#ifndef LWM2M_H__
#define LWM2M_H__

#include "stdint.h"
#include "stdbool.h"
#include "coap_message.h"
#include "coap_codes.h"
#include "sdk_config.h"
#include "app_trace.h"
#include "sdk_os.h"
#include "iot_errors.h"

/**
 * @defgroup iot_lwm2m_log Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests. These are intended for debugging purposes and
 *          can be disabled by defining the LWM2M_ENABLE_LOGS to 0.
 * @note That if ENABLE_DEBUG_LOG_SUPPORT is disabled, having LWM2M_ENABLE_LOGS has no effect.
 * @{
 */
#if (LWM2M_ENABLE_LOGS == 1)

#define LWM2M_TRC  app_trace_log                   /**< Used for getting trace of execution in the module. */
#define LWM2M_DUMP app_trace_dump                  /**< Used for dumping octet information to get details of bond information etc. */
#define LWM2M_ERR  app_trace_log                   /**< Used for logging errors in the module. */

#else // LWM2M_ENABLE_LOGS

#define LWM2M_TRC(...)                             /**< Diasbles traces. */
#define LWM2M_DUMP(...)                            /**< Diasbles dumping of octet streams. */
#define LWM2M_ERR(...)                             /**< Diasbles error logs. */

#endif // LWM2M_ENABLE_LOGS

/**
 * @defgroup iot_coap_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case the need to use an alternative architecture arises.
 * @{
 */
#define LWM2M_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_lwm2m_mutex)                                 /**< Lock module using mutex */
#define LWM2M_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_lwm2m_mutex)                               /**< Unlock module using mutex */
/** @} */

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          LWM2M_DISABLE_API_PARAM_CHECK should be set to 0 to enable these checks.
 *
 * @{
 */
#if (LWM2M_DISABLE_API_PARAM_CHECK == 0)

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL | IOT_LWM2M_ERR_BASE);                                              \
    }
#else

#define NULL_PARAM_CHECK(PARAM)

#endif // LWM2M_DISABLE_API_PARAM_CHECK

#define LWM2M_REQUEST_TYPE_BOOTSTRAP        1
#define LWM2M_REQUEST_TYPE_REGISTER         2
#define LWM2M_REQUEST_TYPE_UPDATE           3
#define LWM2M_REQUEST_TYPE_DEREGISTER       4

#endif // LWM2M_H__

/** @} */
