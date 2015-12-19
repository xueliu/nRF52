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
 * @file iot_file_pstorage_raw.h
 * @brief FILE port for direct flash access.
 */

#ifndef IOT_FILE_PSTORAGE_RAW_H__
#define IOT_FILE_PSTORAGE_RAW_H__

#include "iot_file_port.h"

/**
 * @defgroup iot_file_pstorage_raw IoT file port for direct flash access.
 * @{
 * @ingroup iot_file
 * @brief Macro function which simplifies file setup process and file type assigning function.
 */

/**
 * @brief This macro function configures passed file as a direct flash access file.
 */
#define IOT_FILE_PSTORAGE_RAW_INIT(p_iot_file, p_file_name, start_address, size, callback)  \
    do {                                                                                    \
        (p_iot_file)->p_filename    = p_file_name;                                          \
        (p_iot_file)->cursor        = IOT_FILE_INVALID_CURSOR;                              \
        (p_iot_file)->p_arg         = (void *)start_address;                                \
        (p_iot_file)->buffer_size   = size;                                                 \
        (p_iot_file)->p_buffer      = NULL;                                                 \
        (p_iot_file)->file_size     = 0;                                                    \
        (p_iot_file)->p_callback    = callback;                                             \
        iot_file_pstorage_raw_assign(p_iot_file);                                           \
    } while(0)

/**
 * @brief This function is used to assign correct callbacks and file type to passed IoT File instance.
 */
void iot_file_pstorage_raw_assign(iot_file_t * p_file);

#endif

/** @} */
