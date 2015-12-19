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
 
/**@file
 *
 * @defgroup iot_dfu_bootloader_api Bootloader API
 * @ingroup iot_sdk_dfu
 * @{     
 *
 * @brief Bootloader module interface.
 */

#ifndef BOOTLOADER_API_H__
#define BOOTLOADER_API_H__

#include "nordic_common.h"
#include "dfu_types.h"

/**@brief Function for initializing the Bootloader.
 * 
 * @retval  NRF_SUCCESS If bootloader was succesfully initialized, else any error code recieved
 *          when initializing underlying flash storage module.
 */
uint32_t bootloader_init(void);

/**@brief Function for validating application region in flash.
 * 
 * @param[in]  app_addr  Address to the region in flash where the application is stored.
 * 
 * @retval True  If Application region is valid.
 * @retval False If Application region is not valid.
 */
bool bootloader_app_is_valid(uint32_t app_addr);

/**@brief Function for exiting bootloader and booting into application.
 *
 * @details This function will disable SoftDevice and all interrupts before jumping to application.
 *          The SoftDevice vector table base for interrupt forwarding will be set the application
 *          address.
 *
 * @param[in] app_addr Address to the region where the application is stored.
 */
uint32_t bootloader_app_start(uint32_t app_addr);

/**@brief Function getting state of DFU update in progress.
 *        After a successful firmware transfer to the SWAP area, the system restarts in order to
 *        copy data and check if they are valid.
 *
 * @retval  True   A firmware update is in progress. This indicates that SWAP area 
 *                 is filled with new firmware.
 * @retval  False  No firmware update is in progress.
 */
bool bootloader_dfu_update_in_progress(void);

/**@brief Function for continuing the Device Firmware Update.
 * 
 * @retval     NRF_SUCCESS If the final stage of SoftDevice update was successful, else an error
 *             indicating reason of failure.
 */
uint32_t bootloader_dfu_update_continue(void);

#endif // BOOTLOADER_API_H__

/**@} */
