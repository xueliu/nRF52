/* Copyright (c)  2014 Nordic Semiconductor. All Rights Reserved.
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

/** @file nrf_driver.h
 *
 * @brief Implements lwip stack driver on nRF.
 *
 * @details Implements lwip stack driver on nRF.
 */

#ifndef NRF_PLATFORM_PORT_H__
#define NRF_PLATFORM_PORT_H__

/**@brief Prescaler value for timer module to get a tick of about 1ms.
 *
 * @note Applications using lwIP and this driver, must use this value of prescaler when
 *       initializing the timer module.
 */
#define NRF_DRIVER_TIMER_PRESCALER    31

/**@biref Initializes the driver for LwIP stack. */
uint32_t nrf_driver_init(void);

/**@biref API assumed to be implemented by the application to handle interface up event. */
void nrf_driver_interface_up(void);

/**@biref API assumed to be implemented by the application to handle interface down event. */
void nrf_driver_interface_down(void);

#endif //NRF_PLATFORM_PORT_H__
