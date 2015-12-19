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

#ifndef SOCKET_COMMON_H__
#define SOCKET_COMMON_H__

#include "app_trace.h"

/**
 * @defgroup socket_debug_log Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests. These are intended for debugging purposes and
 *          can be disabled by defining the SOCKET_LOGS_ENABLE.
 * @note That if ENABLE_DEBUG_LOG_SUPPORT is disabled, having SOCKET_LOGS_ENABLE has no effect.
 * @{
 */

#if SOCKET_LOGS_ENABLE == 1
#ifdef __GNUC__

#define SOCKET_TRACE(...) do {              \
    (void) fprintf( stderr, __VA_ARGS__ );  \
} while (0)

#else

#define SOCKET_TRACE  app_trace_log                  /**< Used for getting trace of execution in the module. */
#define SOCKET_DUMP   app_trace_dump                 /**< Used for dumping octet information to get details of bond information etc. */
#define SOCKET_ERR    app_trace_log                  /**< Used for logging errors in the module. */

#endif

#else // SOCKET_LOGS_ENABLE

#define SOCKET_TRACE(...)                            /**< Disables traces. */
#define SOCKET_DUMP(...)                             /**< Disables dumping of octet streams. */
#define SOCKET_ERR(...)                              /**< Disables error logs. */

#endif // SOCKET_LOGS_ENABLE


#endif
