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
 * @defgroup iot_dfu_types Types and definitions.
 * @ingroup iot_sdk_dfu
 * @{
 *
 * @brief Device Firmware Update module type and definitions.
 */

#ifndef DFU_TYPES_H__
#define DFU_TYPES_H__

#include <stdint.h>
#include "nrf_sdm.h"
#include "nrf.h"
#include "app_util.h"

#define NRF_UICR_BOOT_START_ADDRESS     (NRF_UICR_BASE + 0x14)                                     /**< Register where the bootloader start address is stored in the UICR register. */

#define CODE_REGION_1_START             SD_SIZE_GET(MBR_SIZE)                                      /**< This field should correspond to the size of Code Region 0, (which is identical to Start of Code Region 1), found in UICR.CLEN0 register. This value is used for compile safety, as the linker will fail if application expands into bootloader. Runtime, the bootloader will use the value found in UICR.CLEN0. */
#define SOFTDEVICE_REGION_START         MBR_SIZE                                                   /**< This field should correspond to start address of the bootloader, found in UICR.RESERVED, 0x10001014, register. This value is used for sanity check, so the bootloader will fail immediately if this value differs from runtime value. The value is used to determine max application size for updating. */

#define BOOTLOADER_REGION_START         0x0007D000                                                 /**< This field should correspond to start address of the bootloader, found in UICR.RESERVED, 0x10001014, register. This value is used for sanity check, so the bootloader will fail immediately if this value differs from runtime value. The value is used to determine max application size for updating. */
#define BOOTLOADER_SETTINGS_ADDRESS     0x0007F000                                                 /**< The field specifies the page location of the bootloader settings address. */
#define CODE_PAGE_SIZE                  0x1000                                                     /**< Size of a flash codepage. Used for size of the reserved flash space in the bootloader region. Will be runtime checked against NRF_UICR->CODEPAGESIZE to ensure the region is correct. */

#define DFU_REGION_TOTAL_SIZE           (BOOTLOADER_REGION_START - CODE_REGION_1_START)            /**< Total size of the region between SD and Bootloader. */

#define DFU_APP_DATA_RESERVED           0x0000                                                     /**< Size of Application Data that must be preserved between application updates. This value must be a multiple of page size. Page size is 0x400 (1024d) bytes, thus this value must be 0x0000, 0x0400, 0x0800, 0x0C00, 0x1000, etc. */
#define DFU_BANK_PADDING                (DFU_APP_DATA_RESERVED % (2 * CODE_PAGE_SIZE))             /**< Padding to ensure that image size banked is always page sized. */
#define DFU_IMAGE_MAX_SIZE_FULL         (DFU_REGION_TOTAL_SIZE - DFU_APP_DATA_RESERVED)            /**< Maximum size of an application, excluding save data from the application. */
#define DFU_IMAGE_MAX_SIZE_BANKED       ((DFU_REGION_TOTAL_SIZE - \
                                          DFU_APP_DATA_RESERVED - \
                                          DFU_BANK_PADDING) / 2)                                   /**< Maximum size of an application, excluding save data from the application. */

#define DFU_BL_IMAGE_MAX_SIZE           (BOOTLOADER_SETTINGS_ADDRESS - BOOTLOADER_REGION_START)    /**< Maximum size of a bootloader, excluding save data from the current bootloader. */

#define DFU_BANK_0_REGION_START         CODE_REGION_1_START                                        /**< Bank 0 region start. */
#define DFU_BANK_1_REGION_START         (DFU_BANK_0_REGION_START + DFU_IMAGE_MAX_SIZE_BANKED)      /**< Bank 1 region start. */

#define EMPTY_FLASH_MASK                0xFFFFFFFF                                                 /**< Bit mask that defines an empty address in flash. */

#define IS_WORD_SIZED(SIZE) ((SIZE & (sizeof(uint32_t) - 1)) == 0)                                 /**< Macro for checking that the provided is word sized. */

#define BOOTLOADER_PART                 1                                                          /**< Defines bootloader part of software. */
#define SOFTDEVICE_PART                 2                                                          /**< Defines SoftDevice part of software. */
#define APPLICATION_PART                3                                                          /**< Defines application part of software. */

// Safe guard to ensure during compile time that the DFU_APP_DATA_RESERVED is a multiple of page size.
STATIC_ASSERT((((DFU_APP_DATA_RESERVED) & (CODE_PAGE_SIZE - 1)) == 0x00));

/**@brief Structure holding basic device information settings.
 */
typedef struct
{
    uint16_t device_type;                                                                          /**< Device type (2 bytes), for example Heart Rate. This number must be defined by the customer before production. It can be located in UICR or FICR. */
    uint16_t device_rev;                                                                           /**< Device revision (2 bytes), for example major revision 1, minor revision 0. This number must be defined by the customer before production. It can be located in UICR or FICR. */
} dfu_device_info_t;

/** The device info offset can be modified to place the device info settings at a different location.
  * If the customer reserved UICR location is used for other application specific data, the offset
  * must be updated to avoid collision with that data.
  */
/** [DFU UICR DEV offset] */
#define UICR_CUSTOMER_DEVICE_INFO_OFFSET    0x0                                                    /**< Device info offset inside the customer UICR reserved area. Customers may change this value to place the device information in a user-preferred location. */
/** [DFU UICR DEV offset] */

#define UICR_CUSTOMER_RESERVED_OFFSET       0x80                                                   /**< Customer reserved area in the UICR. The area from UICR + 0x80 is reserved for customer usage. */
#define DFU_DEVICE_INFO_BASE                (NRF_UICR_BASE + \
                                             UICR_CUSTOMER_RESERVED_OFFSET + \
                                             UICR_CUSTOMER_DEVICE_INFO_OFFSET)                     /**< The device information base address inside of UICR. */
#define DFU_DEVICE_INFO                     ((dfu_device_info_t *)DFU_DEVICE_INFO_BASE)            /**< The memory mapped structure for device information data. */

#define DFU_DEVICE_TYPE_EMPTY               ((uint16_t)0xFFFF)                                     /**< Mask indicating no device type is present in UICR. 0xFFFF is default flash pattern when not written with data. */
#define DFU_DEVICE_REVISION_EMPTY           ((uint16_t)0xFFFF)                                     /**< Mask indicating no device revision is present in UICR. 0xFFFF is default flash pattern when not written with data. */

#define BANK_VALID_MASK                     (BANK_VALID_APP | BANK_VALID_SD | BANK_VALID_BOOT)     /**< Mask indicating that at least one part of device firmware will be updated after reset. */

/**@brief DFU Bank state code, which indicates wether the bank contains: A valid image, invalid image, or an erased flash.
  */
typedef enum
{
    BANK_VALID_APP   = 0x01,
    BANK_VALID_SD    = 0x02,
    BANK_VALID_BOOT  = 0x04,
    BANK_ERASED      = 0xFE,
    BANK_INVALID_APP = 0xFF,
} bootloader_bank_code_t;

/**@brief Structure holding bootloader settings for application and bank data.
 */
typedef struct
{
    uint8_t                bank_0;          /**< Variable to store if bank 0 contains a valid application. */
    uint16_t               bank_0_crc;      /**< If bank is valid, this field will contain a valid CRC of the total image. */
    uint8_t                bank_1;          /**< Variable to store if bank 1 has been erased/prepared for new image. Bank 1 is only used in Banked Update scenario. */
    uint32_t               bank_0_size;     /**< Size of active image in bank0 if present, otherwise 0. */
    uint32_t               sd_image_size;   /**< Size of SoftDevice image in bank0 if bank_0 code is BANK_VALID_SD. */
    uint32_t               bl_image_size;   /**< Size of Bootloader image in bank0 if bank_0 code is BANK_VALID_SD. */
    uint32_t               app_image_size;  /**< Size of Application image in bank0 if bank_0 code is BANK_VALID_SD. */
    uint32_t               sd_image_start;  /**< Location in flash where SoftDevice image is stored for SoftDevice update. */
} bootloader_settings_t;

#endif // DFU_TYPES_H__

/**@} */
