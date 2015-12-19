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

 /**
 * @file iot_file_port.h
 * @brief Types and definitions used while writing port.
 */
 
#ifndef IOT_FILE_PORT_H__
#define IOT_FILE_PORT_H__

/**
 * @defgroup iot_file_port_defs IoT file definition for port libraries.
 * @{
 * @ingroup iot_file
 * @brief Type definitions for port modules.
 */

/**
 * @defgroup iot_sdk_file Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests. These are intended for debugging purposes and
 *          can be enabled by defining the IOT_FILE_ENABLE_LOGS.
 * @note That if ENABLE_DEBUG_LOG_SUPPORT is disabled, having IOT_FILE_ENABLE_LOGS has no effect.
 * @{
 */
#include "nordic_common.h"
#include "app_trace.h"
#include "sdk_config.h"

#if (IOT_FILE_ENABLE_LOGS == 1)

#define FILE_TRC  app_trace_log                                                                     /**< Used for logging details. */
#define FILE_ERR  app_trace_log                                                                     /**< Used for logging errors in the module. */
#define FILE_DUMP app_trace_dump                                                                    /**< Used for dumping octet information to get details of bond information etc. */

#else  // IOT_FILE_ENABLE_LOGS

#define FILE_TRC(...)                                                                               /**< Diasbles detailed logs. */
#define FILE_ERR(...)                                                                               /**< Diasbles error logs. */
#define FILE_DUMP(...)                                                                              /**< Diasbles dumping of octet streams. */

#endif  // IOT_FILE_ENABLE_LOGS
/** @} */

//NOTE: Port functions don't need to check if p_file exists, because call can be done only on a correct file instance.

#define IOT_FILE_INVALID_CURSOR 0xFFFFFFFF

/**
 * @enum iot_file_type_t
 * @brief Supported file type values.
 */
typedef enum
{
    IOT_FILE_TYPE_UNKNOWN = 0,  /**< File type used for describing not initialized files. */
    IOT_FILE_TYPE_PSTORAGE_RAW, /**< File type used for accessing flash memory. */
    IOT_FILE_TYPE_STATIC        /**< File type used for representing static buffers. */
} iot_file_type_t;

/**
 * @brief iot_file_t structure forward definition.
 */
typedef struct iot_file_struct_t iot_file_t;

/**
 * @brief IoT File fopen() callback type definition.
 */
typedef uint32_t (*iot_fopen_t)(iot_file_t * p_file, uint32_t requested_size);

/**
 * @brief IoT File fwrite() callback type definition.
 */
typedef uint32_t (*iot_fwrite_t)(iot_file_t * p_file, const void * p_data, uint32_t size);

/**
 * @brief IoT File fread() callback type definition.
 */
typedef uint32_t (*iot_fread_t)(iot_file_t * p_file, void * p_data, uint32_t size);

/**
 * @brief IoT File ftell() callback type definition.
 */
typedef uint32_t (*iot_ftell_t)(iot_file_t * p_file, uint32_t * p_cursor);

/**
 * @brief IoT File fseek() callback type definition.
 */
typedef uint32_t (*iot_fseek_t)(iot_file_t * p_file, uint32_t cursor);

/**
 * @brief IoT File frewind() callback type definition.
 */
typedef uint32_t (*iot_frewind_t)(iot_file_t * p_file);

/**
 * @brief IoT File fclose() callback type definition.
 */
typedef uint32_t (*iot_fclose_t)(iot_file_t * p_file);

/**
 * @brief Generic IoT File instance structure.
 */
struct iot_file_struct_t
{
    const char          * p_filename;     /**< Public. String constant describing file name. */
    iot_file_type_t       type;           /**< Public. Type of file. Each type should be added into iot_file_type_t enum. */
    uint32_t              file_size;      /**< Public. Number of valid bytes inside file. */

    uint32_t              cursor;         /**< Internal. Cursor describing which byte will be read/written. */
    uint32_t              buffer_size;    /**< Internal. Size of data buffer. */
    void                * p_buffer;       /**< Internal. Pointer to a data buffer set by calling fopen. */
    void                * p_callback;     /**< Internal. User callback used in order to notify about finished file operations. */
    void                * p_arg;          /**< Internal. User defined argument, used inside the port. */
    iot_fopen_t           open;           /**< Internal. Callback for fopen operation assigned by particular port. */
    iot_fwrite_t          write;          /**< Internal. Callback for fwrite operation assigned by particular port. */
    iot_fread_t           read;           /**< Internal. Callback for fread operation assigned by particular port. */
    iot_ftell_t           tell;           /**< Internal. Callback for ftell operation assigned by particular port. */
    iot_fseek_t           seek;           /**< Internal. Callback for fseek operation assigned by particular port. */
    iot_frewind_t         rewind;         /**< Internal. Callback for frewind operation assigned by particular port. */
    iot_fclose_t          close;          /**< Internal. Callback for fclose operation assigned by particular port. */
};

#endif

/** @} */
