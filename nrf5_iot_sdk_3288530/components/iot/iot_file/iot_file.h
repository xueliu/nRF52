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
 * @file iot_file.h
 * @brief IoT File abstraction API.
 */

#ifndef IOT_FILE_H__
#define IOT_FILE_H__

#include "iot_common.h"
#include "iot_file_port.h"

/**
 * @ingroup iot_sdk_common
 * @addtogroup iot_file IoT File
 * @brief IoT File abstraction definition.
 * @{
 * @defgroup iot_file_abstraction IoT file abstraction
 * @{
 * @brief Structures and public API definition.
 */

/**
 * @enum iot_file_evt_t
 * @brief List of events returned in callback for file ports with asynchrounous operation
 *        like open, read, write and close.
 */
typedef enum {
    IOT_FILE_OPENED,                /**< Event indicates that file has been opened.*/
    IOT_FILE_WRITE_COMPLETE,        /**< Event indicates that single write operation has been completed.*/
    IOT_FILE_READ_COMPLETE,         /**< Event indicates that single read operation has been completed.*/
    IOT_FILE_CLOSED,                /**< Event indicates that file has been closed.*/
    IOT_FILE_ERROR                  /**< Event indicates that file encountered a problem.*/
} iot_file_evt_t;

/**
 * @brief IoT File user callback type definition.
 *
 * @param[in] p_file Reference to an IoT file instance.
 * @param[in] event  Event structure describing what has happened.
 * @param[in] result Result code (should be NRF_SUCCESS for all events except errors).
 * @param[in] p_data Pointer to memory buffer.
 * @param[in] size   Size of data stored in memory buffer.
 * 
 * @retval None.
 */
typedef void (*iot_file_callback_t) (iot_file_t * p_file, iot_file_evt_t event, uint32_t result, void * p_data, uint32_t size);

/**
 * @brief Function to open IoT file. Depending on port, it should allocate required buffer.
 *
 * @param[in] p_file          Pointer to an IoT file instance.
 * @param[in] requested_size  Maximum number of bytes to be read/written.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_file_fopen(iot_file_t * p_file, uint32_t requested_size);

/**
 * @brief Function to write data buffer into a file.
 *
 * @param[in] p_file  Pointer to an IoT file instance.
 * @param[in] p_data  Pointer to data block which will be written into the file.
 * @param[in] size    Number of bytes to be written.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_file_fwrite(iot_file_t * p_file, const void * p_data, uint32_t size);

/**
 * @brief Function to read data buffer from file.
 *
 * @param[in] p_file  Pointer to an IoT file instance.
 * @param[in] p_data  Pointer to data block to be filled with read data.
 * @param[in] size    Number of bytes to be read.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_file_fread(iot_file_t * p_file, void * p_data, uint32_t size);

/**
 * @brief Function to read current cursor position.
 *
 * @param[in]  p_file    Pointer to an IoT file instance.
 * @param[out] p_cursor  Current value of a cursor.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_file_ftell(iot_file_t * p_file, uint32_t * p_cursor);

/**
 * @brief Function to set current cursor position.
 *
 * @param[in] p_file  Pointer to an IoT file instance.
 * @param[in] cursor  New cursor value.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_file_fseek(iot_file_t * p_file, uint32_t cursor);

/**
 * @brief Function to rewind file cursor.
 *
 * @param[in]  p_file  Pointer to an IoT file instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_file_frewind(iot_file_t * p_file);

/**
 * @brief Function to close IoT file. Depending on port, it should free used buffer.
 *
 * @param[in]  p_file  Pointer to an IoT file instance.
 *
 * @retval NRF_SUCCESS on successful execution of procedure, else an error code indicating reason
 *                     for failure.
 */
uint32_t iot_file_fclose(iot_file_t * p_file);

#endif

/** @} */
/** @} */
