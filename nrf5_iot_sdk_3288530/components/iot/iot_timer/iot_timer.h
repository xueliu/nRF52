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

 /** @file
 *
 * @defgroup iot_timer IoT Timer
 * @{
 * @ingroup iot_sdk_common
 * @brief Timekeeping for other modules.
 *
 * @details The IoT Timer stores the value of the wall clock, which represents the time elapsed 
 *          since startup (or an integer overflow). The unit of timekeeping is milliseconds. 
 *          The IoT Timer module is conceived to be platform independent, therefore, it does not 
 *          have an internal tick source. An external source has to update the wall clock of the 
 *          IoT Timer at regular intervals. 
 *          Other modules can query the current value of the wall clock and/or can act as clients 
 *          of the IoT Timer by subscribing to callbacks that are repeated at regular intervals.
 *          You can configure the module by changing the @c sdk_config.h configuration file.
 */

#ifndef IOT_TIMER_H__
#define IOT_TIMER_H__

#include <stdint.h>
#include "nrf_error.h"

#define SEC_TO_MILLISEC(PARAM) (PARAM * 1000)

/**@brief The type of an instant in milliseconds. */
typedef uint32_t iot_timer_time_in_ms_t;

/**@brief IoT Timer client callback type. 
 *
 * @param[in] wall_clock_value The value of the wall clock that triggered the callback. 
 *
 * @retval None.
 *
 */
typedef void (*iot_timer_tick_cb)(iot_timer_time_in_ms_t wall_clock_value);

/**@brief IoT Timer client structure. 
 *
 * @note  @ref cb_interval cannot be zero.
 * @note  @ref cb_interval must be greater or equal to @ref IOT_TIMER_RESOLUTION_IN_MS.
 * @note  If greater, @ref cb_interval must be an integral multiple of @ref IOT_TIMER_RESOLUTION_IN_MS.
 */
typedef struct
{
    iot_timer_tick_cb      iot_timer_callback;      /**< Callback procedure of the client. */
    iot_timer_time_in_ms_t cb_interval;             /**< Interval between repeated callbacks to the client. */
} iot_timer_client_t;

/**@brief IoT Timer client list structure. */
typedef struct
{
    uint8_t                    client_list_length;  /**< Total number of clients. */
    const iot_timer_client_t * p_client_list;       /**< Pointer to the constant array of clients or NULL. */
} iot_timer_clients_list_t;

/**@brief Function for setting the list of clients that subscribe for repeated callbacks from 
 *        the module.
 *
 * @note  To minimize drift between client callbacks, the callback function of each client 
 *        should be designed in a way that the duration of execution does not vary and is as short 
 *        as possible. 
 *
 * @param[in]  p_list_of_clients  Address of the client list. Can be NULL to cancel all subscriptions. 
 *                                To see what parameters are valid, please see the description 
 *                                of @ref iot_timer_client_t.
 *
 * @retval     NRF_SUCCESS              Address of the list of clients successfully updated.
 * @retval     NRF_ERROR_INVALID_PARAM  If any member of the client list has NULL as a callback 
 *                                      procedure or the interval for repeated callbacks is smaller
 *                                      or equal to @ref IOT_TIMER_RESOLUTION_IN_MS or it is not
 *                                      an integral multiple of @ref IOT_TIMER_RESOLUTION_IN_MS.
 *
 */
uint32_t iot_timer_client_list_set(const iot_timer_clients_list_t * p_list_of_clients);

/**@brief Function for updating the wall clock. 
 * 
 * @details The application designer must ensure that this function is called at regular intervals, 
 *          which is set in the @c sdk_config.h configuration file. 
 *          If the updated value of the wall clock is an integral multiple of the callback interval
 *          of any clients of the module, the callback procedure of the client is executed. 
 *
 * @note  The interrupt that triggers the update of the wall clock should have a high relative
 *        priority to minimize inaccuracy. 
 * 
 * @retval     NRF_SUCCESS      Wall clock successfully updated.
 *
 */
uint32_t iot_timer_update(void);

/**@brief Function for getting the current wall clock value.
 *
 * @note  The accuracy of timekeeping is limited by the resolution, as set in the @c sdk_config.h 
 *        configuration file. 
 *
 * @param[out] p_elapsed_time   Value of the wall clock. Time in milliseconds elapsed since startup
 *                              (or an integer overflow). 
 *
 * @retval     NRF_SUCCESS      Query successful.
 * @retval     NRF_ERROR_NULL   If @b p_elapsed_time is a NULL pointer. 
 *
 */
uint32_t iot_timer_wall_clock_get(iot_timer_time_in_ms_t * p_elapsed_time);

/**@brief Function for getting the difference between the current and an older wall clock value.
 *
 * @note  The accuracy of calculation is limited by the wall clock resolution, as set in 
 *        the @c sdk_config.h configuration file. 
 * @note  The time difference can only be calculated correctly if only at most one integer overflow
 *        of the wall clock has occured since the past wall clock value was obtained. 
 *
 * @param[in]  p_past_time      Past value of the wall clock. Has to be an integral multiple of
 *                              @ref IOT_TIMER_RESOLUTION_IN_MS or zero.
 * @param[out] p_delta_time     Time elapsed since @b p_past_time in milliseconds. 
 *
 * @retval     NRF_SUCCESS              Query successful.
 * @retval     NRF_ERROR_NULL           If @b p_past_time or @b p_delta_time is a NULL pointer. 
 * @retval     NRF_ERROR_INVALID_PARAM  If @b p_past_time points to a value that is not an integral
 *                                      multiple of @ref IOT_TIMER_RESOLUTION_IN_MS.
 *
 */
uint32_t iot_timer_wall_clock_delta_get(iot_timer_time_in_ms_t * p_past_time,   \
                                        iot_timer_time_in_ms_t * p_delta_time);

#endif // IOT_TIMER_H__

/**@} */
