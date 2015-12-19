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

#include <string.h>
#include "sdk_config.h"
#include "iot_file.h"
#include "iot_file_pstorage_raw.h"
#include "iot_common.h"
#include "app_util.h"
#include "pstorage.h"
#include "nrf.h"
#include "pstorage_platform.h"
#include "mem_manager.h"

/**
 * @defgroup tftp_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case need arises to use an alternative architecture.
 * @{
 */
#define FPSRAW_MUTEX_LOCK()       SDK_MUTEX_LOCK(m_fpsraw_mutex)                                    /**< Lock module using mutex */
#define FPSRAW_MUTEX_UNLOCK()     SDK_MUTEX_UNLOCK(m_fpsraw_mutex)                                  /**< Unlock module using mutex */
/** @} */

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

#define LOCKED 1

#define LOCK_CHECK(p_file)                                            \
    if (((fpstorage_instance_t *)(p_file->p_buffer))->lock == LOCKED) \
    {                                                                 \
        return (NRF_ERROR_BUSY | IOT_FILE_ERR_BASE);                  \
    }

/**@brief Locks a file access lock for certain instance. */
#define LOCK(p_file) ((fpstorage_instance_t *)(p_file->p_buffer))->lock = LOCKED

/**@brief Unlocks a file access lock for certain instance. */
#define UNLOCK(p_file) ((fpstorage_instance_t *)(p_file->p_buffer))->lock = 0

/**@brief Unique cursor value that represents state between fopen and first fwrite/fread. */
#define PSTORAGE_OPENED_CURSOR IOT_FILE_INVALID_CURSOR - 1

#define FPSTORAGE_OFFSET (sizeof(fpstorage_instance_t))

typedef struct fpstorage_instance_t fpstorage_instance_t;
struct fpstorage_instance_t
{
    pstorage_handle_t         handle;
    pstorage_module_param_t   param;
    uint32_t                  lock;
    iot_file_t              * p_file;
    uint8_t                 * p_op_buffer;
    fpstorage_instance_t    * p_next;
};

static fpstorage_instance_t   m_flash;
static fpstorage_instance_t * p_root_instance;
static uint32_t               m_callback_set = 0;

SDK_MUTEX_DEFINE(m_fpsraw_mutex)                                                                    /**< Mutex variable. Currently unused, this declaration does not occupy any space in RAM. */


/**@brief Notify application on asynchronous event. */
static void app_notify(iot_file_t     * p_file, 
                       iot_file_evt_t   event,
                       uint32_t         result,
                       void           * p_data,
                       uint32_t         size)
{
    iot_file_callback_t *p_cb = (iot_file_callback_t *) &p_file->p_callback;

    if (p_file->p_callback)
    {
        FPSRAW_MUTEX_UNLOCK();
        (*p_cb)(p_file, event, result, p_data, size);
        FPSRAW_MUTEX_LOCK();
    }
}


/**@brief Find free file instance. */
static uint32_t find_instance(fpstorage_instance_t ** pp_instance, pstorage_handle_t * p_handle)
{
    p_root_instance = &m_flash;

    FILE_TRC("[FILE][PSRaw]: Find Instance: %lx\r\n", p_handle->block_id);

    while (p_root_instance->handle.block_id != p_handle->block_id)
    {
        if (p_root_instance->p_next == NULL)
        {
            break;
        }

        p_root_instance = p_root_instance->p_next;
    }

    if (p_root_instance->handle.block_id != p_handle->block_id)
    {
        return (NRF_ERROR_NOT_FOUND | IOT_FILE_ERR_BASE);
    }
    else
    {
        (*pp_instance) = p_root_instance;

        return NRF_SUCCESS;
    }
}


/**@brief Find last file instance. */
static void find_last(fpstorage_instance_t ** pp_instance)
{
    while ((*pp_instance)->p_next != NULL)
    {
        (*pp_instance) = (*pp_instance)->p_next;
    }

    return;
}


/**@brief PStorage event notification handler. */
static void pstorage_handler(pstorage_handle_t * p_handle,
                             uint8_t             op_code,
                             uint32_t            result,
                             uint8_t           * p_data,
                             uint32_t            data_len)
{
    fpstorage_instance_t * p_instance;
    uint32_t               err_code;

    FPSRAW_MUTEX_LOCK();

    err_code = find_instance(&p_instance, p_handle);
    if (err_code != NRF_SUCCESS)
    {
        FPSRAW_MUTEX_UNLOCK();
        FILE_ERR("[FILE][PSRaw]: Cannot find instance based on PStorage callback.\r\n");
        return;
    }

    if (result != NRF_SUCCESS)
    {
        UNLOCK(p_instance->p_file);

        if ((op_code == PSTORAGE_STORE_OP_CODE) || (op_code == PSTORAGE_CLEAR_OP_CODE))
        {
            FILE_TRC("[FILE][PSRaw]: Free buffer.\r\n");
            nrf_free(p_instance->p_op_buffer);
        }

        app_notify(p_instance->p_file, IOT_FILE_ERROR, result, p_data, data_len);

        FPSRAW_MUTEX_UNLOCK();

        FILE_TRC("[FILE][PSRaw]: PStorage error. Reason: %08lx\r\n", result);

        return;
    }

    switch (op_code)
    {
        case PSTORAGE_LOAD_OP_CODE:
            // Load operations are blocking, so there is no need to implement this case.
            break;

        case PSTORAGE_CLEAR_OP_CODE:
            if (p_instance->p_file->cursor == 0)
            {
                // Clear operation occur just before first write operation.
                // Request update of first block. Block size is (size) bytes. Offset is equal to (cursor).
                err_code = pstorage_raw_store(&p_instance->handle,
                                              p_instance->p_op_buffer,
                                              p_instance->param.block_size,
                                              p_instance->p_file->cursor);
                if (err_code != NRF_SUCCESS)
                {
                    FILE_ERR("[FILE][PSRaw]: PStorage store failed. Reason: %08lx.\r\n", err_code);
                    FILE_ERR("[FILE][PSRaw]:    Params: instance: %p\r\n",
                             &p_instance->handle);
                    FILE_ERR("[FILE][PSRaw]:            address : %lx\r\n",
                             p_instance->handle.block_id);
                    FILE_ERR("[FILE][PSRaw]:            size    : %lu\r\n",
                             p_instance->param.block_size);
                    FILE_ERR("[FILE][PSRaw]:            offset  : %lu\r\n",
                             p_instance->p_file->cursor);

                    UNLOCK(p_instance->p_file);
                    app_notify(p_instance->p_file, IOT_FILE_ERROR, err_code, 
                               p_instance->p_op_buffer, p_instance->param.block_size);
                }
            }

            break;

        case PSTORAGE_STORE_OP_CODE:
            // If store operations ends successfully, move current cursor at the end of stored data.
            p_instance->p_file->cursor += p_instance->param.block_size;

            // Extend file size so it will contain new data.
            if (p_instance->p_file->cursor > p_instance->p_file->file_size)
            {
                p_instance->p_file->file_size = p_instance->p_file->cursor;
            }

            FILE_TRC("[FILE][PSRaw]: Free internal buffer.\r\n");
            nrf_free(p_instance->p_op_buffer);

            UNLOCK(p_instance->p_file);
            app_notify(p_instance->p_file, IOT_FILE_WRITE_COMPLETE, result, p_data, data_len);
            break;

        default:
            break;
    }

    FPSRAW_MUTEX_UNLOCK();
}


/**@brief File instance initialization. */
static uint32_t instance_init(iot_file_t * p_file, uint32_t block_size)
{
    if ((!is_word_aligned((void *)p_file->buffer_size)) || (!is_word_aligned((void *)block_size)))
    {
        p_file->cursor = IOT_FILE_INVALID_CURSOR;
        FILE_ERR("[FILE][PSRaw]: Alignment error during initialization.\r\n");
        return (NRF_ERROR_INVALID_PARAM | IOT_FILE_ERR_BASE);
    }

    p_file->cursor = 0;

    return NRF_SUCCESS;
}


/**@brief Static buffer fwrite port function definition. */
static uint32_t internal_fwrite(iot_file_t * p_file, const void * p_data, uint32_t size)
{
    uint32_t               err_code;
    fpstorage_instance_t * p_instance;

    FILE_TRC("[FILE][PSRaw] >> fwrite.\r\n");

    NULL_PARAM_CHECK(p_data);
    FPSRAW_MUTEX_LOCK();

    LOCK_CHECK(p_file);

    p_instance = (fpstorage_instance_t *)p_file->p_buffer;

    if (p_file->cursor == IOT_FILE_INVALID_CURSOR)
    {
        FILE_ERR("[FILE][PSRaw]: fwrite: File not opened.\r\n");
        return (NRF_ERROR_INVALID_STATE | IOT_FILE_ERR_BASE);
    }

    if (!is_word_aligned((void *)size))
    {
        p_file->cursor = IOT_FILE_INVALID_CURSOR;
        FILE_ERR("[FILE][PSRaw]: Data chunk not aligned to word size.\r\n");
        return (NRF_ERROR_INVALID_PARAM | IOT_FILE_ERR_BASE);
    }

    // First fwrite call - clear persistent memory.
    if (p_file->cursor == PSTORAGE_OPENED_CURSOR)
    {
        err_code = instance_init(p_file, size);
        if (err_code != NRF_SUCCESS)
        {
            FPSRAW_MUTEX_UNLOCK();
            FILE_ERR("[FILE][PSRaw]: Initialization error.\r\n");
            FILE_TRC("[FILE][PSRaw]: << fwrite.\r\n");
            return err_code;
        }

        // Clear required pages of pstorage.
        err_code = pstorage_raw_clear(&p_instance->handle, p_file->buffer_size);
        if (err_code != NRF_SUCCESS)
        {
            p_file->cursor = IOT_FILE_INVALID_CURSOR;
            FPSRAW_MUTEX_UNLOCK();
            FILE_ERR("[FILE][PSRaw]: Cannot clear flash memory.\r\n");
            FILE_TRC("[FILE][PSRaw]: << fwrite.\r\n");

            return err_code;
        }
    }

    if ((size == 0) || (p_file->buffer_size < (p_file->cursor + size)))
    {
        FPSRAW_MUTEX_UNLOCK();
        FILE_ERR("[FILE][PSRaw]: Buffer size - %lu, has %lu used bytes, requested for %lu bytes.\r\n",
                 p_file->buffer_size, p_file->cursor, size);
        FILE_TRC("[FILE][PSRaw]: << fwrite.\r\n");
        return (NRF_ERROR_DATA_SIZE | IOT_FILE_ERR_BASE);
    }

    // Allocate buffer for fwrite operation.
    p_instance->param.block_size = size;
    FILE_TRC("[FILE][PSRaw]: ALLOC buffer.\r\n");

    err_code = nrf_mem_reserve(&p_instance->p_op_buffer, (uint32_t *)&p_instance->param.block_size);
    if (err_code != NRF_SUCCESS)
    {
        FILE_ERR("[FILE][PSRaw]: Cannot allocate temporary buffer (%lu bytes). Reason: %08lx.\r\n",
                 size, err_code);

        app_notify(p_file, IOT_FILE_ERROR, err_code, p_instance->p_op_buffer, size);
        FPSRAW_MUTEX_UNLOCK();
        FILE_TRC("[FILE][PSRaw] << fwrite.\r\n");

        return err_code;
    }

    // Restore block size after alloc call.
    p_instance->param.block_size = size;
    // Copy data into allocated buffer.
    memcpy(p_instance->p_op_buffer, p_data, size);

    LOCK(p_file);

    // First block will be written inside PStorage callback (flash cleared event).
    if (p_file->cursor != 0)
    {
        FILE_ERR("[FILE][PSRaw]:              address : %lx\r\n", p_instance->handle.block_id);
        FILE_ERR("[FILE][PSRaw]:              offset  : %lu\r\n", p_instance->p_file->cursor);
        // Request update of one block. Block size is (size) bytes. Offset is equal to (cursor).
        err_code = pstorage_raw_store(&p_instance->handle,
                                      p_instance->p_op_buffer,
                                      size,
                                      p_instance->p_file->cursor);
        if (err_code != NRF_SUCCESS)
        {
            FILE_ERR("[FILE][PSRaw]: PStorage store failed. Reason: %08lx.\r\n", err_code);
            FILE_ERR("[FILE][PSRaw]:      Params: instance: %p\r\n", &p_instance->handle);
            FILE_ERR("[FILE][PSRaw]:              address : %lx\r\n", p_instance->handle.block_id);
            FILE_ERR("[FILE][PSRaw]:              size    : %lu\r\n", size);
            FILE_ERR("[FILE][PSRaw]:              offset  : %lu\r\n", p_instance->p_file->cursor);
        
            UNLOCK(p_file);
            app_notify(p_file, IOT_FILE_ERROR, err_code, p_instance->p_op_buffer, size);
            FPSRAW_MUTEX_UNLOCK();
            FILE_TRC("[FILE][PSRaw] << fwrite.\r\n");

            return err_code;
        }
    }

    FPSRAW_MUTEX_UNLOCK();

    FILE_TRC("[FILE][PSRaw] << fwrite.\r\n");

    return NRF_SUCCESS;
}


/**@brief Static buffer fread port function definition. */
static uint32_t internal_fread(iot_file_t * p_file, void * p_data, uint32_t size)
{
    uint32_t               err_code = NRF_SUCCESS;
    fpstorage_instance_t * p_instance;

    FILE_TRC("[FILE][PSRaw] >> fread.\r\n");

    NULL_PARAM_CHECK(p_data);
    FPSRAW_MUTEX_LOCK();

    LOCK_CHECK(p_file);

    if (p_file->cursor == IOT_FILE_INVALID_CURSOR)
    {
        FILE_ERR("[FILE][PSRaw]: fwrite: File not opened.\r\n");
        return (NRF_ERROR_INVALID_STATE | IOT_FILE_ERR_BASE);
    }

    // First fread call - set block size.
    if (p_file->cursor == PSTORAGE_OPENED_CURSOR)
    {
        err_code = instance_init(p_file, size);
        if (err_code != NRF_SUCCESS)
        {
            FPSRAW_MUTEX_UNLOCK();
            FILE_ERR("[FILE][PSRaw]: Initialization error.\r\n");
            FILE_TRC("[FILE][PSRaw]: << fread.\r\n");
            return err_code;
        }
    }

    if ((size == 0) || (p_file->file_size < (p_file->cursor + size)))
    {
        FPSRAW_MUTEX_UNLOCK();
        FILE_ERR("[FILE][PSRaw]: Invalid size - %lu, has %lu used bytes, requested for %lu bytes.\r\n",
                 p_file->file_size, p_file->cursor, size);
        FILE_TRC("[FILE][PSRaw] << fread.\r\n");
        return (NRF_ERROR_DATA_SIZE | IOT_FILE_ERR_BASE);
    }

    p_instance = (fpstorage_instance_t *)p_file->p_buffer;

    LOCK(p_file);

    // Request to read (size) bytes from block at an offset of (cursor) bytes.
    memcpy(p_data, (((uint8_t *)p_instance->handle.block_id) + p_file->cursor), size);

    p_file->cursor += size;

    UNLOCK(p_file);

    app_notify(p_file, IOT_FILE_READ_COMPLETE, NRF_SUCCESS, p_data, size);
    FPSRAW_MUTEX_UNLOCK();

    FILE_TRC("[FILE][PSRaw] << fread.\r\n");

    return NRF_SUCCESS;
}


/**@brief Static ftell port function definition. */
static uint32_t internal_ftell(iot_file_t * p_file, uint32_t * p_cursor)
{
    NULL_PARAM_CHECK(p_cursor);
    FPSRAW_MUTEX_LOCK();

    if (p_file->cursor == PSTORAGE_OPENED_CURSOR)
    {
        *p_cursor = 0;
    }
    else
    {
        *p_cursor = p_file->cursor;
    }

    FPSRAW_MUTEX_UNLOCK();

    return NRF_SUCCESS;
}


/**@brief Static fseek port function definition. */
static uint32_t internal_fseek(iot_file_t * p_file, uint32_t cursor)
{
    uint32_t err_code;

    FILE_TRC("[FILE][PSRaw]: >> fseek.\r\n");
    
    FPSRAW_MUTEX_LOCK();
    LOCK_CHECK(p_file);

    if (p_file->cursor == IOT_FILE_INVALID_CURSOR)
    {
        FPSRAW_MUTEX_UNLOCK();
        FILE_ERR("[FILE][PSRaw]: Invalid file cursor.\r\n");
        FILE_TRC("[FILE][PSRaw]: << fseek.\r\n");
        return (NRF_ERROR_INVALID_STATE | IOT_FILE_ERR_BASE);
    }

    if ((cursor == IOT_FILE_INVALID_CURSOR) ||
        (cursor > p_file->buffer_size)     ||
        (!is_word_aligned((void *)cursor)))
    {
        FPSRAW_MUTEX_UNLOCK();
        FILE_ERR("[FILE][PSRaw]: Invalid argument.\r\n");
        FILE_TRC("[FILE][PSRaw]: << fseek.\r\n");
        return (NRF_ERROR_INVALID_PARAM | IOT_FILE_ERR_BASE);

    }

    // First operation on file.
    if (p_file->cursor == PSTORAGE_OPENED_CURSOR)
    {
        err_code = instance_init(p_file, cursor);
        if (err_code != NRF_SUCCESS)
        {
            FPSRAW_MUTEX_UNLOCK();
            FILE_ERR("[FILE][PSRaw]: Initialization error.\r\n");
            FILE_TRC("[FILE][PSRaw] << fseek.\r\n");
            return err_code;
        }
    }

    p_file->cursor = cursor;

    FPSRAW_MUTEX_UNLOCK();

    FILE_TRC("[FILE][PSRaw]: << fseek.\r\n");
    return NRF_SUCCESS;
}


/**@brief Static frewind port function definition. */
static uint32_t internal_frewind(iot_file_t * p_file)
{
    FILE_TRC("[FILE][PSRaw]: >> frewind.\r\n");
    
    FPSRAW_MUTEX_LOCK();
    LOCK_CHECK(p_file);

    if (p_file->cursor == IOT_FILE_INVALID_CURSOR)
    {
        FPSRAW_MUTEX_UNLOCK();

        FILE_ERR("[FILE][PSRaw]: Invalid file state for frewind operation.\r\n");
        FILE_TRC("[FILE][PSRaw]: << frewind.\r\n");

        return (NRF_ERROR_INVALID_STATE | IOT_FILE_ERR_BASE);
    }
    else if (p_file->cursor != PSTORAGE_OPENED_CURSOR)
    {
        LOCK(p_file);
        p_file->cursor = 0;
        UNLOCK(p_file);
    }

    FPSRAW_MUTEX_UNLOCK();

    FILE_TRC("[FILE][PSRaw]: << frewind.\r\n");

    return NRF_SUCCESS;
}


/**@brief Static fopen port function definition. */
static uint32_t internal_fopen(iot_file_t * p_file, uint32_t requested_size)
{
    uint32_t               err_code;
    uint32_t               start_address;
    fpstorage_instance_t * p_instance;

    start_address = (uint32_t)p_file->p_arg;

    FILE_TRC("[FILE][PSRaw] >> fopen.\r\n");

    FPSRAW_MUTEX_LOCK();

    if (p_file->cursor != IOT_FILE_INVALID_CURSOR)
    {
        FILE_ERR("[FILE][PSRaw]: File already opened.\r\n");
        FPSRAW_MUTEX_UNLOCK();
        FILE_TRC("[FILE][PSRaw] << fopen.\r\n");
        return (NRF_ERROR_INVALID_STATE | IOT_FILE_ERR_BASE);
    }

    if (!is_word_aligned((void *)start_address) ||
        (start_address == 0)                    ||
        (start_address % PSTORAGE_FLASH_PAGE_SIZE != 0))
    {
        FILE_ERR("[FILE][PSRaw]: Invalid start address. Address: %lu, Page size: %d.\r\n",
                 start_address, PSTORAGE_FLASH_PAGE_SIZE);
        FPSRAW_MUTEX_UNLOCK();
        FILE_TRC("[FILE][PSRaw] << fopen.\r\n");
        return (NRF_ERROR_INVALID_PARAM | IOT_FILE_ERR_BASE);
    }

    if((p_file->buffer_size % PSTORAGE_FLASH_PAGE_SIZE != 0) ||
        (start_address + p_file->buffer_size > PSTORAGE_DATA_END_ADDR))
    {
        FILE_ERR("[FILE][PSRaw]: Invalid buffer size. Buffer size: %lu, Page size: %d, Total size: %lu.\r\n",
                 p_file->buffer_size, PSTORAGE_FLASH_PAGE_SIZE, PSTORAGE_DATA_END_ADDR);
        FPSRAW_MUTEX_UNLOCK();
        FILE_TRC("[FILE][PSRaw] << fopen.\r\n");
        return (NRF_ERROR_INVALID_PARAM | IOT_FILE_ERR_BASE);
    }

    if ((requested_size != 0) && (p_file->buffer_size >= requested_size))
    {
        p_file->file_size = requested_size;
    }
    else
    {
        FPSRAW_MUTEX_UNLOCK();
        FILE_TRC("[FILE][PSRaw]: Invalid file size.\r\n");

        return (NRF_ERROR_INVALID_PARAM | IOT_FILE_ERR_BASE);
    }

    // For the first entry, populate m_flash (linked list root).
    if (!m_callback_set)
    {
        FILE_TRC("[FILE][PSRaw]: FLASH file initialization.\r\n");
        p_instance       = &m_flash;
        p_file->p_buffer = p_instance;
        memset(p_file->p_buffer, 0, sizeof(fpstorage_instance_t));
        p_instance->handle.block_id = start_address;

        // Assign callback for raw pstorage operations.
        p_instance->param.cb = pstorage_handler;

        err_code = pstorage_raw_register(&p_instance->param, &p_instance->handle);
        if (err_code != NRF_SUCCESS)
        {
            FILE_ERR("[FILE][PSRaw]: Failed to register pstorage callback.\r\n");
            FPSRAW_MUTEX_UNLOCK();
            FILE_TRC("[FILE][PSRaw] << fopen.\r\n");
            return err_code;
        }

        p_instance->p_next = NULL;
        m_callback_set     = 1;
    }
    // For the first entry, populate m_flash (linked list root).
    else if (p_file->p_buffer == NULL)
    {
        uint32_t size_alloc = sizeof(fpstorage_instance_t);
        FILE_TRC("[FILE][PSRaw]: Regular file initialization.\r\n");
        fpstorage_instance_t * p_last = &m_flash;
        find_last(&p_last);

        err_code = nrf_mem_reserve((uint8_t **)&p_file->p_buffer, &size_alloc);
        if (err_code != NRF_SUCCESS)
        {
            FILE_ERR("[FILE][PSRaw]: Failed to allocate second instance.\r\n");
            p_file->p_buffer = NULL;
            return err_code;
        }

        memcpy(p_file->p_buffer, &m_flash, sizeof(fpstorage_instance_t));
        p_instance = p_file->p_buffer;
        p_instance->handle.block_id = start_address;

        // Assign callback for raw pstorage operations.
        p_instance->param.cb = pstorage_handler;
        p_last->p_next       = p_instance;
        p_instance->p_next   = NULL;
    }
    else
    {
        p_instance = (fpstorage_instance_t *)p_file->p_buffer;
    }

    p_instance->p_file = p_file;
    UNLOCK(p_file);

    p_file->cursor = PSTORAGE_OPENED_CURSOR;
    app_notify(p_file, IOT_FILE_OPENED, NRF_SUCCESS, NULL, 0);

    FPSRAW_MUTEX_UNLOCK();
    FILE_TRC("[FILE][PSRaw] << fopen.\r\n");

    return NRF_SUCCESS;
}


/**@brief Static fclose port function definition. */
static uint32_t internal_fclose(iot_file_t * p_file)
{
    uint32_t err_code = NRF_SUCCESS;

    FILE_TRC("[FILE][PSRaw]: >> fclose.\r\n");

    FPSRAW_MUTEX_LOCK();
    LOCK_CHECK(p_file);

    if (p_file->cursor == IOT_FILE_INVALID_CURSOR)
    {
        FPSRAW_MUTEX_UNLOCK();
        FILE_ERR("[FILE][PSRaw]: File not opened.\r\n");
        FILE_TRC("[FILE][PSRaw]: << fclose.\r\n");
        return (NRF_ERROR_INVALID_STATE | IOT_FILE_ERR_BASE);
    }

    app_notify(p_file, IOT_FILE_CLOSED, err_code, NULL, 0);
    p_file->cursor = IOT_FILE_INVALID_CURSOR;

    FPSRAW_MUTEX_UNLOCK();

    FILE_TRC("[FILE][PSRaw]: << fclose.\r\n");

    return NRF_SUCCESS;
}


/**@brief This function is used to assign correct callbacks and file type to passed file instance. */
void iot_file_pstorage_raw_assign(iot_file_t * p_file)
{
    if (p_file == NULL)
    {
        return;
    }

    FPSRAW_MUTEX_LOCK();

    p_file->type   = IOT_FILE_TYPE_PSTORAGE_RAW;
    p_file->write  = internal_fwrite;
    p_file->read   = internal_fread;
    p_file->tell   = internal_ftell;
    p_file->seek   = internal_fseek;
    p_file->rewind = internal_frewind;
    p_file->open   = internal_fopen;
    p_file->close  = internal_fclose;

    p_file->p_buffer = NULL;

    FPSRAW_MUTEX_UNLOCK();
}
