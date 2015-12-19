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
 
#include "nordic_common.h"
#include "sdk_common.h"
#include "sdk_config.h"
#include "iot_common.h"
#include "app_trace.h"
#include "iot_context_manager.h"

/**
 * @defgroup iot_context_manager_log Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests. These are intended for debugging purposes and
 *          can be enabled by setting the IOT_CONTEXT_MANAGER_ENABLE_LOGS to 1.
 * @note That if ENABLE_DEBUG_LOG_SUPPORT is disabled, having IOT_CONTEXT_MANAGER_ENABLE_LOGS has no effect.
 * @{
 */
#if (IOT_CONTEXT_MANAGER_ENABLE_LOGS == 1)

#define CM_TRC  app_trace_log                                                                       /**< Used for getting trace of execution in the module. */
#define CM_DUMP app_trace_dump                                                                      /**< Used for dumping octet information to get details of bond information etc. */
#define CM_ERR  app_trace_log                                                                       /**< Used for logging errors in the module. */

#else // IOT_CONTEXT_MANAGER_ENABLE_LOGS

#define CM_TRC(...)                                                                                 /**< Diasbles traces. */
#define CM_DUMP(...)                                                                                /**< Diasbles dumping of octet streams. */
#define CM_ERR(...)                                                                                 /**< Diasbles error logs. */

#endif // IOT_CONTEXT_MANAGER_ENABLE_LOGS
/** @} */


/**@brief Parameter maximum values. */
#define CID_VALUE_MAX                15
#define PREFIX_LENGTH_VALUE_MAX      128

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          IOT_CONTEXT_MANAGER_DISABLE_API_PARAM_CHECK should be defined to disable these checks.
 *
 * @{
 */
#if (IOT_CONTEXT_MANAGER_DISABLE_API_PARAM_CHECK == 0)

/**@brief Macro to check is module is initialized before requesting one of the module procedures. */
#define VERIFY_MODULE_IS_INITIALIZED()                                                             \
        if (m_initialization_state == false)                                                        \
        {                                                                                          \
            return (SDK_ERR_MODULE_NOT_INITIALZED | IOT_CONTEXT_MANAGER_ERR_BASE);                 \
        }
        
/**@brief Macro to check is module is initialized before requesting one of the module procedures. */
#define VERIFY_MODULE_IS_INITIALIZED_NULL()                                                        \
        if (m_initialization_state == false)                                                        \
        {                                                                                          \
            return NULL;                                                                           \
        }
        
/**
 * @brief Verify NULL parameters are not passed to API by application.
 */
#define NULL_PARAM_CHECK(PARAM)                                                                    \
        if ((PARAM) == NULL)                                                                       \
        {                                                                                          \
            return (NRF_ERROR_NULL | IOT_CONTEXT_MANAGER_ERR_BASE);                                \
        }
        
/**
 * @brief Verify CID has valid value.
 */
#define VERIFY_CID_VALUE(CID)                                                                      \
        if (!((CID) <= CID_VALUE_MAX))                                                             \
        {                                                                                          \
           return (NRF_ERROR_INVALID_PARAM | IOT_CONTEXT_MANAGER_ERR_BASE);                        \
        }
        
/**
 * @brief Verify prefix length value.
 */
#define VERIFY_PREFIX_LEN_VALUE(PREFIX_LEN)                                                        \
        if (!(PREFIX_LEN <= PREFIX_LENGTH_VALUE_MAX))                                              \
        {                                                                                          \
           return (NRF_ERROR_INVALID_PARAM | IOT_CONTEXT_MANAGER_ERR_BASE);                        \
        }

#else // IOT_IOT_CONTEXT_MANAGER_DISABLE_API_PARAM_CHECK

#define VERIFY_MODULE_IS_INITIALIZED()
#define VERIFY_MODULE_IS_INITIALIZED_NULL()
#define NULL_PARAM_CHECK(PARAM)
#define VERIFY_CID_VALUE(CID)
#define VERIFY_PREFIX_LEN_VALUE(PREFIX_LEN)

#endif //IOT_CONTEXT_MANAGER_DISABLE_API_PARAM_CHECK
/** @} */

/**
 * @defgroup iot_context_manager_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case need arises to use an alternative architecture.
 * @{
 */
#define CM_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_iot_context_manager_mutex)                               /**< Lock module using mutex */
#define CM_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_iot_context_manager_mutex)                             /**< Unlock module using mutex */
/** @} */

/**@brief Context table, managing by IPv6 stack. */
typedef struct
{
    iot_interface_t * p_interface;                                                                  /**< IoT interface pointer. */
    uint8_t           context_count;                                                                /**< Number of valid contexts in the table. */
    iot_context_t     contexts[IOT_CONTEXT_MANAGER_MAX_CONTEXTS];                                   /**< Array of valid contexts. */
}iot_context_table_t;


SDK_MUTEX_DEFINE(m_iot_context_manager_mutex)                                                       /**< Mutex variable. Currently unused, this declaration does not occupy any space in RAM. */
static bool                m_initialization_state = false;                                          /**< Variable to maintain module initialization state. */
static iot_context_table_t m_context_table[IOT_CONTEXT_MANAGER_MAX_TABLES];                         /**< Array of contexts table managed by the module. */

/**@brief Initializes context entry. */
static void context_init(iot_context_t * p_context)
{
    p_context->context_id       = IPV6_CONTEXT_IDENTIFIER_NONE;
    p_context->prefix_len       = 0;
    p_context->compression_flag = false;
    
    memset(p_context->prefix.u8, 0, IPV6_ADDR_SIZE);
}


/**@brief Initializes context table. */
static void context_table_init(uint32_t table_id)
{
    uint32_t index;
    
    for (index = 0; index < IOT_CONTEXT_MANAGER_MAX_CONTEXTS; index++)
    {
        context_init(&m_context_table[table_id].contexts[index]);
        m_context_table[table_id].context_count = 0;
        m_context_table[table_id].p_interface   = NULL;
    }
}


/**@brief Initializes context table. */
static uint32_t context_table_find(const iot_interface_t * p_interface)
{
    uint32_t index;
    
    for (index = 0; index < IOT_CONTEXT_MANAGER_MAX_TABLES; index++)
    {
        if (m_context_table[index].p_interface == p_interface)
        {
            break;
        }
    }
    
    return index;
}


/**@brief Looks up context table for specific context identifier. */
static uint32_t context_find_by_cid(uint32_t         table_id, 
                                    uint8_t          context_id,
                                    iot_context_t ** pp_context)
{
    uint32_t index;

    for (index = 0; index < IOT_CONTEXT_MANAGER_MAX_CONTEXTS; index++)
    {
        if (m_context_table[table_id].contexts[index].context_id == context_id)
        {
              *pp_context = &m_context_table[table_id].contexts[index];
              return NRF_SUCCESS;
        }
    }

    return (NRF_ERROR_NOT_FOUND | IOT_CONTEXT_MANAGER_ERR_BASE);
}


static uint32_t context_find_by_prefix(uint32_t               table_id, 
                                       const ipv6_addr_t    * p_prefix, 
                                       iot_context_t       ** pp_context)
{
    uint32_t index;
    uint32_t context_left;
    uint16_t context_cmp_len;
    iot_context_t * p_best_match = NULL;

    context_left = m_context_table[table_id].context_count;

    for (index = 0; index < IOT_CONTEXT_MANAGER_MAX_CONTEXTS && context_left; index++)
    {
        if (m_context_table[table_id].contexts[index].context_id != IPV6_CONTEXT_IDENTIFIER_NONE &&
            m_context_table[table_id].contexts[index].compression_flag == true)
        {
              if ((context_cmp_len = m_context_table[table_id].contexts[index].prefix_len) < 64)
              {
                  context_cmp_len = 64;
              }

              // Check if address have matched in CID table.
              if (IPV6_ADDRESS_PREFIX_CMP(m_context_table[table_id].contexts[index].prefix.u8,
                                          p_prefix->u8,
                                          context_cmp_len))
              {
                  if (p_best_match == NULL || p_best_match->prefix_len <
                    m_context_table[table_id].contexts[index].prefix_len)
                  {
                      p_best_match = &m_context_table[table_id].contexts[index];
                  }
              } 
          
            // Decrease left context in table, to avoid too many checking.
            context_left--;
        }
    }
    
    if (p_best_match)
    {
        *pp_context = p_best_match;
        return NRF_SUCCESS;
    }

    return (NRF_ERROR_NOT_FOUND | IOT_CONTEXT_MANAGER_ERR_BASE);
}


/**@brief Looks up for first empty entry in the table. */
static uint32_t context_find_free(uint32_t table_id, iot_context_t ** pp_context)
{    
    uint32_t index;
    
    for (index = 0; index < IOT_CONTEXT_MANAGER_MAX_CONTEXTS; index++)
    {
        if (m_context_table[table_id].contexts[index].context_id == IPV6_CONTEXT_IDENTIFIER_NONE)
        {
            *pp_context = &m_context_table[table_id].contexts[index];
            return NRF_SUCCESS;
        } 
    }

    return (NRF_ERROR_NO_MEM | IOT_CONTEXT_MANAGER_ERR_BASE);
}


uint32_t iot_context_manager_init(void)
{
    uint32_t index;

    CM_TRC("[CONTEXT_MANAGER]: >> iot_context_manager_init\r\n");

    SDK_MUTEX_INIT(m_iot_context_manager_mutex);

    CM_MUTEX_LOCK();

    for (index = 0; index < IOT_CONTEXT_MANAGER_MAX_TABLES; index++)
    {
        context_table_init(index);
    }
    
    m_initialization_state = true;

    CM_MUTEX_UNLOCK();

    CM_TRC("[CONTEXT_MANAGER]: << iot_context_manager_init\r\n");

    return NRF_SUCCESS;
}


uint32_t iot_context_manager_table_alloc(const iot_interface_t * p_interface)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_interface);
    
    uint32_t err_code = NRF_SUCCESS;

    CM_TRC("[CONTEXT_MANAGER]: >> iot_context_manager_table_alloc\r\n");

    CM_MUTEX_LOCK();

    const uint32_t table_id = context_table_find(NULL);

    if (table_id != IOT_CONTEXT_MANAGER_MAX_TABLES)
    {
        CM_TRC("[CONTEXT_MANAGER]: Assigned new context table.\r\n");

        // Found a free context table and assign to it.
        m_context_table[table_id].p_interface = (iot_interface_t *)p_interface;
    }
    else
    {
        // No free context table found.
        CM_TRC("[CONTEXT_MANAGER]: No context table found. \r\n");
        err_code = (NRF_ERROR_NO_MEM | IOT_CONTEXT_MANAGER_ERR_BASE);
    }

    CM_MUTEX_UNLOCK();

    CM_TRC("[CONTEXT_MANAGER]: << iot_context_manager_table_alloc\r\n");

    return err_code;    
}


uint32_t iot_context_manager_table_free(const iot_interface_t * p_interface)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_interface);
    
    uint32_t err_code = NRF_SUCCESS;

    CM_TRC("[CONTEXT_MANAGER]: >> iot_context_manager_table_free\r\n");

    SDK_MUTEX_INIT(m_iot_context_manager_mutex);

    CM_MUTEX_LOCK();    
        
    const uint32_t table_id = context_table_find(p_interface);

    if (table_id != IOT_CONTEXT_MANAGER_MAX_TABLES)
    {
        CM_TRC("[CONTEXT_MANAGER]: Found context table assigned to interface.\r\n");

        // Clear context table.
        context_table_init(table_id);
    }
    else
    {
        // No free context table found.
        CM_TRC("[CONTEXT_MANAGER]: No context table found.\r\n");
        err_code = (NRF_ERROR_NOT_FOUND | IOT_CONTEXT_MANAGER_ERR_BASE);
    }

    CM_MUTEX_UNLOCK();

    CM_TRC("[CONTEXT_MANAGER]: << iot_context_manager_table_free\r\n");

    return err_code;    
}


uint32_t iot_context_manager_update(const iot_interface_t * p_interface, 
                                    iot_context_t         * p_context)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_context);
    VERIFY_CID_VALUE(p_context->context_id);
    VERIFY_PREFIX_LEN_VALUE(p_context->prefix_len);
    
    uint32_t        retval   = NRF_SUCCESS;
    uint32_t        err_code = NRF_SUCCESS;
    iot_context_t * p_internal_context;

    CM_TRC("[CONTEXT_MANAGER]: >> iot_context_manager_update\r\n");

    CM_MUTEX_LOCK();

    const uint32_t table_id = context_table_find(p_interface);

    if (table_id != IOT_CONTEXT_MANAGER_MAX_TABLES)
    {
          // Try to find context in context table.
          retval = context_find_by_cid(table_id, p_context->context_id, &p_internal_context);

          if (retval != NRF_SUCCESS)
          {
              err_code = context_find_free(table_id, &p_internal_context);

              // Increase context count.
              if (err_code == NRF_SUCCESS)
              {
                  m_context_table[table_id].context_count++;
              }
          }

          if (err_code == NRF_SUCCESS)
          {
               // Update context table, with parameters from application.
               p_internal_context->context_id       = p_context->context_id;
               p_internal_context->prefix_len       = p_context->prefix_len;
               p_internal_context->compression_flag = p_context->compression_flag;
               memset(p_internal_context->prefix.u8, 0, IPV6_ADDR_SIZE);
               IPV6_ADDRESS_PREFIX_SET(p_internal_context->prefix.u8, p_context->prefix.u8, p_context->prefix_len);
           }
           else
           {
               CM_TRC("[CONTEXT_MANAGER]: No place in context table.\r\n");    
           }
    }
    else
    {
        // No free context table found.
        CM_TRC("[CONTEXT_MANAGER]: No context table found.\r\n");
        err_code = (NRF_ERROR_NOT_FOUND | IOT_CONTEXT_MANAGER_ERR_BASE);
    }

    CM_MUTEX_UNLOCK();

    CM_TRC("[CONTEXT_MANAGER]: << iot_context_manager_update\r\n");

    return err_code;    
}


uint32_t iot_context_manager_remove(const iot_interface_t * p_interface, 
                                    iot_context_t         * p_context)
{    
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_interface);
    NULL_PARAM_CHECK(p_context);
    
    uint32_t err_code  = NRF_SUCCESS;

    CM_TRC("[CONTEXT_MANAGER]: >> iot_context_manager_remove\r\n");

    CM_MUTEX_LOCK();

    const uint32_t table_id = context_table_find(p_interface);

    if (table_id != IOT_CONTEXT_MANAGER_MAX_TABLES)
    {
        if (p_context->context_id != IPV6_CONTEXT_IDENTIFIER_NONE)
        {
            m_context_table[table_id].context_count--;
        }
        
        // Reinit context entry.
        context_init(p_context);         
    }
    else
    {
        // No free context table found.
        CM_TRC("[CONTEXT_MANAGER]: No context table found..\r\n");
        err_code = (NRF_ERROR_NOT_FOUND | IOT_CONTEXT_MANAGER_ERR_BASE);
    }

    CM_MUTEX_UNLOCK();

    CM_TRC("[CONTEXT_MANAGER]: << iot_context_manager_remove\r\n");

    return err_code;
}


uint32_t iot_context_manager_get_by_addr(const iot_interface_t * p_interface, 
                                         const ipv6_addr_t     * p_addr, 
                                         iot_context_t        ** pp_context)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_interface);
    NULL_PARAM_CHECK(p_addr);
    NULL_PARAM_CHECK(pp_context);
    
    uint32_t err_code;

    CM_TRC("[CONTEXT_MANAGER]: >> iot_context_manager_get_by_addr\r\n");

    CM_MUTEX_LOCK();

    const uint32_t table_id = context_table_find(p_interface);

    if (table_id != IOT_CONTEXT_MANAGER_MAX_TABLES)
    {
        err_code = context_find_by_prefix(table_id, p_addr, pp_context);
    }
    else
    {
        // No free context table found.
        CM_TRC("[CONTEXT_MANAGER]: No context table found.\r\n");
        err_code = (NRF_ERROR_NOT_FOUND | IOT_CONTEXT_MANAGER_ERR_BASE);
    }

    CM_MUTEX_UNLOCK();

    CM_TRC("[CONTEXT_MANAGER]: << iot_context_manager_get_by_addr\r\n");

    return err_code;
}


uint32_t iot_context_manager_get_by_cid(const iot_interface_t * p_interface, 
                                        uint8_t           context_id, 
                                        iot_context_t  ** pp_context)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_interface);
    NULL_PARAM_CHECK(pp_context);
    VERIFY_CID_VALUE(context_id);    

    uint32_t err_code;

    CM_TRC("[CONTEXT_MANAGER]: >> iot_context_manager_get_by_cid\r\n");

    CM_MUTEX_LOCK();

    const uint32_t table_id = context_table_find(p_interface);

    if (table_id != IOT_CONTEXT_MANAGER_MAX_TABLES)
    {
        err_code = context_find_by_cid(table_id, context_id, pp_context);
    }
    else
    {
        // No free context table found.
        CM_TRC("[CONTEXT_MANAGER]: No context table found.\r\n");
        err_code = (NRF_ERROR_NOT_FOUND | IOT_CONTEXT_MANAGER_ERR_BASE);
    }

    CM_MUTEX_UNLOCK();

    CM_TRC("[CONTEXT_MANAGER]: << iot_context_manager_get_by_cid\r\n");

    return err_code;
}
