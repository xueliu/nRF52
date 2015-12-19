/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
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
#ifndef SDK_CONFIG_H__
#define SDK_CONFIG_H__

/**
 * @defgroup sdk_config SDK Configuration
 * @{
 * @ingroup sdk_common
 * @{
 * @details All parameters that allow configuring/tuning the SDK based on application/ use case
 *          are defined here.
 */


/**
 * @defgroup mem_manager_config Memory Manager Configuration
 * @{
 * @addtogroup sdk_config
 * @{
 * @details This section defines configuration of memory manager module.
 */

/**
 * @brief Maximum memory blocks identified as 'small' blocks.
 *
 * @details Maximum memory blocks identified as 'small' blocks.
 *          Minimum value : 0 (Setting to 0 disables all 'small' blocks)
 *          Maximum value : 255
 *          Dependencies  : None.
 */
#define  MEMORY_MANAGER_SMALL_BLOCK_COUNT                  9

/**
 * @brief Size of each memory blocks identified as 'small' block.
 *
 * @details Size of each memory blocks identified as 'small' block.
 *          Memory block are recommended to be word-sized.
 *          Minimum value : 32
 *          Maximum value : A value less than the next pool size. If only small pool is used, this
 *                          can be any value based on availability of RAM.
 *          Dependencies  : MEMORY_MANAGER_SMALL_BLOCK_COUNT is non-zero.
 */
#define  MEMORY_MANAGER_SMALL_BLOCK_SIZE                   128

/**
 * @brief Maximum memory blocks identified as 'medium' blocks.
 *
 * @details Maximum memory blocks identified as 'medium' blocks.
 *          Minimum value : 0 (Setting to 0 disables all 'medium' blocks)
 *          Maximum value : 255
 *          Dependencies  : None.
 */
#define  MEMORY_MANAGER_MEDIUM_BLOCK_COUNT                 5

/**
 * @brief Size of each memory blocks identified as 'medium' block.
 *
 * @details Size of each memory blocks identified as 'medium' block.
 *          Memory block are recommended to be word-sized.
 *          Minimum value : A value greater than the small pool size if defined, else 32.
 *          Maximum value : A value less than the next pool size. If only medium pool is used, this
 *                          can be any value based on availability of RAM.
 *          Dependencies  : MEMORY_MANAGER_MEDIUM_BLOCK_COUNT is non-zero.
 */
#define  MEMORY_MANAGER_MEDIUM_BLOCK_SIZE                  256

/**
 * @brief Maximum memory blocks identified as 'medium' blocks.
 *
 * @details Maximum memory blocks identified as 'medium' blocks.
 *          Minimum value : 0 (Setting to 0 disables all 'large' blocks)
 *          Maximum value : 255
 *          Dependencies  : None.
 */
#define  MEMORY_MANAGER_LARGE_BLOCK_COUNT                  3

/**
 * @brief Size of each memory blocks identified as 'medium' block.
 *
 * @details Size of each memory blocks identified as 'medium' block.
 *          Memory block are recommended to be word-sized.
 *          Minimum value : A value greater than the small &/ medium pool size if defined, else 32.
 *          Maximum value : Any value based on availability of RAM.
 *          Dependencies  : MEMORY_MANAGER_MEDIUM_BLOCK_COUNT is non-zero.
 */
#define  MEMORY_MANAGER_LARGE_BLOCK_SIZE                   1024

/**
 * @brief Disable debug trace in the module.
 *
 * @details Set this define to 1 to enable debug trace in the module, else set to 0.
 *          Possible values : 0 or 1.
 *          Dependencies    : ENABLE_DEBUG_LOG_SUPPORT. If this flag is not defined, no
 *                            trace is observed even if this define is set to 1.
 */
#define MEM_MANAGER_ENABLE_LOGS                           0

/**
 * @brief Disables API parameter checks in the module.
 *
 * @details Set this define to 1 to disable checks on API parameters in the module.
 *          API parameter checks are added to ensure right parameters are passed to the
 *          module. These checks are useful during development phase but be redundant
 *          once application is developed. Disabling this can result in some code saving.
 *          Possible values : 0 or 1.
 *          Dependencies    : None.
 */
#define MEM_MANAGER_DISABLE_API_PARAM_CHECK                0
/** @} */
/** @} */


/**
 * @defgroup iot_context_manager Context Manager Configurations.
 * @{
 * @addtogroup iot_config
 * @{
 * @details This section defines configuration of Context Manager.
 */

/**
 * @brief Disable debug trace in the module.
 *
 * @details Set this define to 1 to enable debug trace in the module, else set to 0.
 *          Possible values : 0 or 1.
 *          Dependencies    : ENABLE_DEBUG_LOG_SUPPORT. If this flag is not defined, no
 *                            trace is observed even if this define is set to 1.
 */
#define IOT_CONTEXT_MANAGER_ENABLE_LOGS                     0

/**
 * @brief Disables API parameter checks in the module.
 *
 * @details Set this define to 1 to disable checks on API parameters in the module.
 *          API parameter checks are added to ensure right parameters are passed to the
 *          module. These checks are useful during development phase but be redundant
 *          once application is developed. Disabling this can result in some code saving.
 *          Possible values : 0 or 1.
 *          Dependencies    : None.
 */
#define IOT_CONTEXT_MANAGER_DISABLE_API_PARAM_CHECK        0

/**
 * @brief Maximum number of supported context identifiers.
 *
 * @details Maximum value of 16 is preferable to correct decompression.
 *          Minimum value : 1
 *          Maximum value : 16
 *          Dependencies  : None.
 */
#define  IOT_CONTEXT_MANAGER_MAX_CONTEXTS                  16

/**
 * @brief Maximum number of supported context's table.
 *
 * @details If value is equal to BLE_IPSP_MAX_CHANNELS then all interface will have
 *          its own table which is preferable.
 *          Minimum value : 1
 *          Maximum value : BLE_IPSP_MAX_CHANNELS
 *          Dependencies  : None.
 */
#define  IOT_CONTEXT_MANAGER_MAX_TABLES                    1
/** @} */
/** @} */


/**
 * @defgroup lwip_nrf_driver nRF lwIP driver
 * @{
 * @addtogroup iot_config
 * @{
 * @details This section defines configuration of nRF lwIP driver.
 */
 
/**
 * @brief Disable debug trace in the module.
 *
 * @details Set this define to 1 to enable debug trace in the module, else set to 0.
 *          Possible values : 0 or 1.
 *          Dependencies    : ENABLE_DEBUG_LOG_SUPPORT. If this flag is not defined, no
 *                            trace is observed even if this define is set to 1.
 */
#define NRF_LWIP_DRIVER_ENABLE_LOGS                        0

/** @} */
/** @} */

/**
 * @defgroup iot_socket_config Socket API Configuration
 * @{
 * @addtogroup iot_config
 * @{
 * @details This section defines configuration of socket module.
 */
/**
 * @brief Maximum sockets managed by the module.
 *
 * @details Maximum sockets managed by the module.
 *          Minimum value : 1
 *          Maximum value : 255
 *          Dependencies  : None.
 */
#define SOCKET_MAX_SOCKET_COUNT                              1

/**
 * @brief Enable debug trace in the module.
 *
 * @details Set this define to 1 to enable debug trace in the module, else set to 0.
 *          Possible values : 0 or 1.
 *          Dependencies    : ENABLE_DEBUG_LOG_SUPPORT. If this flag is not defined, no
 *                            trace is observed even if this define is set to 1.
 */
#define SOCKET_LOGS_ENABLE                                   1

/**
 * @brief Enables API parameter checks in the module.
 *
 * @details Set this define to 1 to enable checks on API parameters in the module.
 *          API parameter checks are added to ensure right parameters are passed to the
 *          module. These checks are useful during development phase but be redundant
 *          once application is developed. Disabling this can result in some code saving.
 *          Possible values : 0 or 1.
 *          Dependencies    : None.
 */
#define SOCKET_ENABLE_API_PARAM_CHECK                        1

/**
 * @brief Enables Nordic IPv6 stack support in socket API.
 *
 * @details Set this define to 0 to disable Nordic IPv6 support in socket API. Disabling it
 *          can result in some code saving if you are not using it in your application.
 *          Possible values : 0 or 1.
 *          Dependencies    : None.
 */
#define SOCKET_IPV6_ENABLE                                   0

/**
 * @brief Enables LWIP stack support in socket API.
 *
 * @details Set this define to 0 to disable LWIP support in socket API. Disabling it
 *          can result in some code saving if you are not using it in your application.
 *          Possible values : 0 or 1.
 *          Dependencies    : None.
 */
#define SOCKET_LWIP_ENABLE                                   1

/**
 * @brief Enables automatic medium setup from socket API.
 *
 * @details The automatic medium setup allows the socket API to setup the entire bluetooth stack
 *          so that you can run PC applications using the socket API with no modifications needed.
 *          Set this define to 1 to enable automatic medium setup. Disabling it is required
 *          if you need to do any custom modifications to medium.
 *          Possible values : 0 or 1.
 *          Dependencies    : None.
 */
#define SOCKET_MEDIUM_ENABLE                                 1

/** @} */
/** @} */

/** @} */
/** @} */

#endif // SDK_CONFIG_H__
