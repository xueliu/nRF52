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

#include <stdlib.h>
#include "cJSON.h"
#include "cJSON_iot_hooks.h"
#include "sdk_config.h"
#include "mem_manager.h"
#include "app_trace.h"

#if (CJSON_HOOKS_ENABLE_LOGS == 1)

#define CJSON_HOOKS_LOG  app_trace_log

#else // CJSON_HOOKS_ENABLE_LOGS

#define CJSON_HOOKS_LOG(...)

#endif // CJSON_HOOKS_ENABLE_LOGS

static cJSON_Hooks m_iot_hooks;

/**@brief malloc() function definition. */
static void * malloc_fn_hook(size_t sz)
{    
    return nrf_malloc(sz);
}

/**@brief free() function definition. */
static void free_fn_hook(void * p_ptr)
{
    nrf_free((uint8_t * ) p_ptr);
}

/**@brief Initialize cJSON by assigning function hooks. */
void cJSON_Init(void)
{
    m_iot_hooks.malloc_fn = malloc_fn_hook;
    m_iot_hooks.free_fn   = free_fn_hook;

    cJSON_InitHooks(&m_iot_hooks);
}
