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
 * @defgroup iot_timer IoT SDK Timer
 * @{
 * @addtogroup iot_config
 * @{
 * @details This section defines configuration of the IoT Timer.
 */
 
/**
 * @brief Wall clock resolution in milliseconds.
 *
 * @details The wall clock of the IoT Timer module has to be updated from an external source at
 *          regular intervals. This define needs to be set to the interval between updates.
 *          Minimum value   : 1.
 *          Dependencies    : None.
 *
 */
#define IOT_TIMER_RESOLUTION_IN_MS                       100

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
#define IOT_TIMER_DISABLE_API_PARAM_CHECK                  0
/** @} */
/** @} */

#endif // SDK_CONFIG_H__
