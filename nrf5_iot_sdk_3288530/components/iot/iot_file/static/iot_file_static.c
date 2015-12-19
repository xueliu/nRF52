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
#include "iot_file_static.h"
#include "iot_common.h"
#include <string.h>

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          IOT_FILE_STATIC_DISABLE_API_PARAM_CHECK should be set to 0 to enable these checks.
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

#define CHECK_CURSOR(PARAM)                                         \
    if ((PARAM) == IOT_FILE_INVALID_CURSOR)                         \
    {                                                               \
        return  (NRF_ERROR_INVALID_STATE | IOT_FILE_ERR_BASE);      \
    }

/**@brief Static buffer fwrite port function definition. */
static uint32_t internal_fwrite(iot_file_t * p_file, const void * p_data, uint32_t size)
{
    NULL_PARAM_CHECK(p_data);
    CHECK_CURSOR(p_file->cursor);

    if ((size + p_file->cursor) > p_file->buffer_size) 
    {
        return (NRF_ERROR_DATA_SIZE | IOT_FILE_ERR_BASE);
    }
    
    memcpy(((uint8_t *)p_file->p_buffer + p_file->cursor), p_data, size);
    p_file->cursor += size;
    
    if (p_file->cursor > p_file->file_size)
    {
        p_file->file_size = p_file->cursor;
    }
    
    return NRF_SUCCESS;
}


/**@brief Static buffer fread port function definition. */
static uint32_t internal_fread(iot_file_t * p_file, void * p_data, uint32_t size)
{
    NULL_PARAM_CHECK(p_data);
    CHECK_CURSOR(p_file->cursor);

    if (size + p_file->cursor > p_file->file_size) 
    {
        return (NRF_ERROR_DATA_SIZE | IOT_FILE_ERR_BASE);
    }
    
    memcpy(p_data, (uint8_t *)p_file->p_buffer + p_file->cursor, size);
    p_file->cursor += size;
    
    return NRF_SUCCESS;
}


/**@brief Static ftell port function definition. */
static uint32_t internal_ftell(iot_file_t * p_file, uint32_t * p_cursor)
{
    NULL_PARAM_CHECK(p_cursor);
    CHECK_CURSOR(p_file->cursor);

    *p_cursor = p_file->cursor;
    
    return NRF_SUCCESS;
}


/**@brief Static fseek port function definition. */
static uint32_t internal_fseek(iot_file_t * p_file, uint32_t cursor)
{
    CHECK_CURSOR(p_file->cursor);

    if (cursor > p_file->buffer_size)
    {
        FILE_ERR("[FILE][STATIC]: Cursor outside file!.\r\n");
        return (NRF_ERROR_INVALID_PARAM | IOT_FILE_ERR_BASE);
    }
    
    p_file->cursor = cursor;
    
    return NRF_SUCCESS;
}


/**@brief Static frewind port function definition. */
static uint32_t internal_frewind(iot_file_t * p_file)
{
    CHECK_CURSOR(p_file->cursor);

    p_file->cursor = 0;
    
    return NRF_SUCCESS;
}


/**@brief Static fopen port function definition. */
static uint32_t internal_fopen(iot_file_t * p_file, uint32_t requested_size)
{
    p_file->cursor = 0;

    if (requested_size != 0)
    {
        p_file->file_size = requested_size;
    }

    return NRF_SUCCESS;
}


/**@brief Static fclose port function definition. */
static uint32_t internal_fclose(iot_file_t * p_file)
{
    p_file->cursor = IOT_FILE_INVALID_CURSOR;

    return NRF_SUCCESS;
}


/**@brief This function is used to assign correct callbacks and file type to passed IoT File instance. */
void iot_file_static_assign(iot_file_t * p_file)
{
    if (p_file == NULL)
    {
        return;
    }
    
    p_file->type   = IOT_FILE_TYPE_STATIC;
    p_file->write  = internal_fwrite;
    p_file->read   = internal_fread;
    p_file->tell   = internal_ftell;
    p_file->seek   = internal_fseek;
    p_file->rewind = internal_frewind;
    p_file->open   = internal_fopen;
    p_file->close  = internal_fclose;
}

