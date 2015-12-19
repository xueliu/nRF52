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
 * @defgroup ble_sdk_app_bootloader_main main.c
 * @{
 * @ingroup dfu_bootloader_api
 * @brief Bootloader project main file.
 *
 * -# Receive start data packet. 
 * -# Based on start packet, prepare NVM area to store received data. 
 * -# Receive data packet. 
 * -# Validate data packet.
 * -# Write Data packet to NVM.
 * -# If not finished - Wait for next packet.
 * -# Receive stop data packet.
 * -# Activate Image, boot application.
 *
 */

#include "bsp.h" 
#include "nrf_mbr.h"
#include "pstorage.h"
#include "softdevice_handler.h"
#include "dfu_types.h"
#include "bootloader_api.h"

#if LEDS_NUMBER < 1
#error "Not enough LEDs on board"
#endif

#define UPDATE_IN_PROGRESS_LED          BSP_LED_2                                                   /**< Led used to indicate that DFU is active. */

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze 
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] file_name   File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(0xDEADBEEF, line_num, p_file_name);
}


/**@brief Function for initialization of LEDs.
 */
static void leds_init(void)
{
    nrf_gpio_cfg_output(UPDATE_IN_PROGRESS_LED);
    nrf_gpio_pin_set(UPDATE_IN_PROGRESS_LED);
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void sys_evt_dispatch(uint32_t event)
{
    pstorage_sys_event_handler(event);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 *
 * @param[in] init_softdevice  true if SoftDevice should be initialized. The SoftDevice must only 
 *                             be initialized if a chip reset has occured. Soft reset from 
 *                             application must not reinitialize the SoftDevice.
 */
static void sd_init(void)
{
    uint32_t         err_code;
    sd_mbr_command_t com = {SD_MBR_COMMAND_INIT_SD, };

    err_code = sd_mbr_command(&com);
    APP_ERROR_CHECK(err_code);

    err_code = sd_softdevice_vector_table_base_set(BOOTLOADER_REGION_START);
    APP_ERROR_CHECK(err_code);
   
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, false);

    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for bootloader main entry.
 */
int main(void)
{
    uint32_t err_code;

    leds_init();

    // This check ensures that the defined fields in the bootloader corresponds with actual
    // setting in the nRF52 chip.
    APP_ERROR_CHECK_BOOL(*((uint32_t *)NRF_UICR_BOOT_START_ADDRESS) == BOOTLOADER_REGION_START);
    APP_ERROR_CHECK_BOOL(NRF_FICR->CODEPAGESIZE == CODE_PAGE_SIZE);

    // Initialize Soft Device.
    sd_init();

    err_code = bootloader_init();
    APP_ERROR_CHECK(err_code);

    if (bootloader_dfu_update_in_progress())
    {
        nrf_gpio_pin_clear(UPDATE_IN_PROGRESS_LED);

        err_code = bootloader_dfu_update_continue();
        APP_ERROR_CHECK(err_code);

        nrf_gpio_pin_set(UPDATE_IN_PROGRESS_LED);
    }

    if (!bootloader_dfu_update_in_progress())
    {
       if (bootloader_app_is_valid(DFU_BANK_0_REGION_START))
       {
            // Select a bank region to use as application region.
            // @note: Only applications running from DFU_BANK_0_REGION_START is supported.
            err_code = bootloader_app_start(DFU_BANK_0_REGION_START);
            APP_ERROR_CHECK(err_code);
       }
       else
       {
            // Application is not valid or has not been found.
            APP_ERROR_HANDLER(NRF_ERROR_NOT_FOUND);
       }
    }

    NVIC_SystemReset();
}
