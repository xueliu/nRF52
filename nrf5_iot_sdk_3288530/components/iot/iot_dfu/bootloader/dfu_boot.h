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
 * @defgroup iot_dfu_bootloader_image Bootloader images
 * @ingroup iot_sdk_dfu
 * @{     
 *
 * @brief Internal Device Firmware Update module interface.
 */

#ifndef APP_DFU_BOOT_H__
#define APP_DFU_BOOT_H__

#include "dfu_types.h"

/**@brief Function for loading the bootloader settings.
 *
 * @details This function is defined as an interface function. The implementation of this is system
 *          specific and needs to be implemented by a user module.
 *
 * @param[out] p_bootloader_settings Current bootloader settings.
 */
void bootloader_settings_load(bootloader_settings_t * p_bootloader_settings);

/**@brief Function for saving the bootloader settings.
 *
 * @details This function is defined as an interface function. The implementation of this is system
 *          specific and needs to be implemented by a user module.
 * @param[in]  p_bootloader_settings New bootloader settings. 
 */
void bootloader_settings_save(bootloader_settings_t * p_bootloader_settings);

/**@brief Function for checking if new bootloader will be installed.
 *
 * @details The function will also compare the copied version against the firmware source,
 *          and mark it as installed if comparisson is a success before returning.
 *
 * @return NRF_SUCCESS if install was successful. NRF_ERROR_NULL if the images differs.
 */
uint32_t dfu_bl_image_validate(void);

/**@brief Function for validating that new application has been correctly installed.
 *
 * @details The function will also compare the copied version against the firmware source,
 *          and mark it as installed if comparisson is a success before returning.
 *
 * @return NRF_SUCCESS if install was successful. NRF_ERROR_NULL if the images differs.
 */
uint32_t dfu_app_image_validate(void);

/**@brief Function for validating that new SoftDevice has been correctly installed.
 *
 * @details The function will also compare the copied version against the firmware source,
 *          and mark it as installed if comparisson is a success before returning.
 *
 * @return NRF_SUCCESS if install was successful. NRF_ERROR_NULL if the images differs.
 */
uint32_t dfu_sd_image_validate(void);

/**@brief Function for swapping existing bootloader with newly received.
 *
 * @note This function will not return any error code but rather reset the chip after.
 *       If an error occurs during copy of bootloader, this will be resumed by the system on
 *       power-on again.
 *
 * @return NRF_SUCCESS on succesfull swapping. For error code please refer to
 *         sd_mbr_command_copy_bl_t.
 */
uint32_t dfu_bl_image_swap(void);

/**@brief Function for swapping existing SoftDevice with newly received.
 *
 * @return NRF_SUCCESS on succesfull swapping. For error code please refer to
 *         sd_mbr_command_copy_sd_t.
 */
uint32_t dfu_sd_image_swap(void);

/**@brief Function for swapping existing application with newly received.
 *
 * @return NRF_SUCCESS on succesfull swapping. For error code please refer to
 *         sd_mbr_command_copy_sd_t.
 */
uint32_t dfu_app_image_swap(void);

#endif // APP_DFU_BOOT_H__

/** @} */
