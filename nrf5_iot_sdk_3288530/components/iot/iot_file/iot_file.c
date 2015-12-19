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
 
#include "sdk_config.h"
#include "iot_file.h"
#include "iot_common.h"

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          IOT_FILE_DISABLE_API_PARAM_CHECK should be set to 0 to enable these checks.
 *
 * @{
 */
#if (IOT_FILE_DISABLE_API_PARAM_CHECK == 0)

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM)                                     \
    if ((PARAM) == NULL)                                            \
    {                                                               \
        return (NRF_ERROR_NULL | IOT_FILE_ERR_BASE);                \
    }

#else // IOT_FILE_DISABLE_API_PARAM_CHECK

#define NULL_PARAM_CHECK(PARAM)

#endif // IOT_FILE_DISABLE_API_PARAM_CHECK
/** @} */


/**
 * @brief Function to open IoT file. Depending on port, it should allocate required buffer.
 */
uint32_t iot_file_fopen(iot_file_t * p_file, uint32_t requested_size) 
{
    NULL_PARAM_CHECK(p_file);
  
    if (p_file->open != NULL)
    {
        return p_file->open(p_file, requested_size);
    }
    else
    {
        p_file->buffer_size = requested_size;
        return API_NOT_IMPLEMENTED;
    }
}


/**
 * @brief Function to write data buffer into a file.
 */
uint32_t iot_file_fwrite(iot_file_t * p_file, const void * p_data, uint32_t size) 
{
    NULL_PARAM_CHECK(p_file);
  
    if (p_file->write != NULL)
    {
        return p_file->write(p_file, p_data, size);
    }
    else
    {
        return API_NOT_IMPLEMENTED;
    }
}


/**
 * @brief Function to read data buffer from file.
 */
uint32_t iot_file_fread(iot_file_t * p_file, void * p_data, uint32_t size) 
{
    NULL_PARAM_CHECK(p_file);
  
    if (p_file->read != NULL)
    {
        return p_file->read(p_file, p_data, size);
    }
    else
    {
        return API_NOT_IMPLEMENTED;
    }
}


/**
 * @brief Function to read current cursor position.
 */
uint32_t iot_file_ftell(iot_file_t * p_file, uint32_t * p_cursor) 
{
    NULL_PARAM_CHECK(p_file);
  
    if (p_file->tell != NULL)
    {
        return p_file->tell(p_file, p_cursor);
    }
    else
    {
        return API_NOT_IMPLEMENTED;
    }
}


/**
 * @brief Function to set current cursor position.
 */
uint32_t iot_file_fseek(iot_file_t * p_file, uint32_t cursor) 
{
    NULL_PARAM_CHECK(p_file);
  
    if (p_file->seek != NULL)
    {
        return p_file->seek(p_file, cursor);
    }
    else
    {
        return API_NOT_IMPLEMENTED;
    }
}


/**
 * @brief Function to rewind file cursor.
 */
uint32_t iot_file_frewind(iot_file_t * p_file) 
{
    NULL_PARAM_CHECK(p_file);
  
    if (p_file->rewind != NULL)
    {
        return p_file->rewind(p_file);
    }
    else
    {
        return API_NOT_IMPLEMENTED;
    }
}


/**
 * @brief Function to close IoT file. Depending on port, it should free used buffer.
 */
uint32_t iot_file_fclose(iot_file_t * p_file) 
{
    NULL_PARAM_CHECK(p_file);
  
    if (p_file->close != NULL)
    {
        return p_file->close(p_file);
    }
    else
    {
        return API_NOT_IMPLEMENTED;
    }
}

