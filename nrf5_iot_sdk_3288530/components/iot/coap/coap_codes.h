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

/** @file coap_codes.h
 *
 * @defgroup iot_sdk_coap_codes CoAP Codes
 * @ingroup iot_sdk_coap
 * @{
 * @brief CoAP message and response codes.
 */

#ifndef COAP_CODES_H__
#define COAP_CODES_H__

#define COAP_CODE(c, dd) \
    ((((c) & 0x7) << 5) | (((1 ## dd) - 100) & 0x1F)) 
/**
    @note macro adds 1xx to the number in order to prevent dd from being 
          interpreted as octal.
*/

/** @brief CoAP Message codes
*/
typedef enum
{
    // CoAP Empty Message
    COAP_CODE_EMPTY_MESSAGE                  = COAP_CODE(0,00),  /**< CoAP code 0.00, Decimal: 0, Hex: 0x00. */

    // CoAP Method Codes
    COAP_CODE_GET                            = COAP_CODE(0,01),  /**< CoAP code 0.01, Decimal: 1, Hex: 0x01. */
    COAP_CODE_POST                           = COAP_CODE(0,02),  /**< CoAP code 0.02, Decimal: 2, Hex: 0x02. */
    COAP_CODE_PUT                            = COAP_CODE(0,03),  /**< CoAP code 0.03, Decimal: 3, Hex: 0x03. */
    COAP_CODE_DELETE                         = COAP_CODE(0,04),  /**< CoAP code 0.04, Decimal: 4, Hex: 0x04. */

    // CoAP Success Response Codes
    COAP_CODE_201_CREATED                    = COAP_CODE(2,01),  /**< CoAP code 2.01, Decimal: 65, Hex: 0x41. */
    COAP_CODE_202_DELETED                    = COAP_CODE(2,02),  /**< CoAP code 2.02, Decimal: 66, Hex: 0x42. */
    COAP_CODE_203_VALID                      = COAP_CODE(2,03),  /**< CoAP code 2.03, Decimal: 67, Hex: 0x43. */
    COAP_CODE_204_CHANGED                    = COAP_CODE(2,04),  /**< CoAP code 2.04, Decimal: 68, Hex: 0x44. */
    COAP_CODE_205_CONTENT                    = COAP_CODE(2,05),  /**< CoAP code 2.05, Decimal: 69, Hex: 0x45. */
    COAP_CODE_231_CONTINUE                   = COAP_CODE(2,31),  /**< CoAP code 2.31, Decimal: 95, Hex: 0x5F. */

    // CoAP Client Error Response Codes
    COAP_CODE_400_BAD_REQUEST                = COAP_CODE(4,00),  /**< CoAP code 4.00, Decimal: 128, Hex: 0x80. */
    COAP_CODE_401_UNAUTHORIZED               = COAP_CODE(4,01),  /**< CoAP code 4.01, Decimal: 129, Hex: 0x81. */
    COAP_CODE_402_BAD_OPTION                 = COAP_CODE(4,02),  /**< CoAP code 4.02, Decimal: 130, Hex: 0x82. */
    COAP_CODE_403_FORBIDDEN                  = COAP_CODE(4,03),  /**< CoAP code 4.03, Decimal: 131, Hex: 0x83. */
    COAP_CODE_404_NOT_FOUND                  = COAP_CODE(4,04),  /**< CoAP code 4.04, Decimal: 132, Hex: 0x84. */
    COAP_CODE_405_METHOD_NOT_ALLOWED         = COAP_CODE(4,05),  /**< CoAP code 4.05, Decimal: 133, Hex: 0x85. */
    COAP_CODE_406_NOT_ACCEPTABLE             = COAP_CODE(4,06),  /**< CoAP code 4.06, Decimal: 134, Hex: 0x86. */
    COAP_CODE_408_REQUEST_ENTITY_INCOMPLETE  = COAP_CODE(4,08),  /**< CoAP code 4.08, Decimal: 136, Hex: 0x88. */
    COAP_CODE_412_PRECONDITION_FAILED        = COAP_CODE(4,12),  /**< CoAP code 4.12, Decimal: 140, Hex: 0x8C. */
    COAP_CODE_413_REQUEST_ENTITY_TOO_LARGE   = COAP_CODE(4,13),  /**< CoAP code 4.13, Decimal: 141, Hex: 0x8D. */
    COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT = COAP_CODE(4,15),  /**< CoAP code 4.15, Decimal: 143, Hex: 0x8F. */

    // CoAP Server Error Response Codes
    COAP_CODE_500_INTERNAL_SERVER_ERROR      = COAP_CODE(5,00),  /**< CoAP code 5.00, Decimal: 160, Hex: 0xA0. */
    COAP_CODE_501_NOT_IMPLEMENTED            = COAP_CODE(5,01),  /**< CoAP code 5.01, Decimal: 161, Hex: 0xA1. */
    COAP_CODE_502_BAD_GATEWAY                = COAP_CODE(5,02),  /**< CoAP code 5.02, Decimal: 162, Hex: 0xA2. */
    COAP_CODE_503_SERVICE_UNAVAILABLE        = COAP_CODE(5,03),  /**< CoAP code 5.03, Decimal: 163, Hex: 0xA3. */
    COAP_CODE_504_GATEWAY_TIMEOUT            = COAP_CODE(5,04),  /**< CoAP code 5.04, Decimal: 164, Hex: 0xA4. */
    COAP_CODE_505_PROXYING_NOT_SUPPORTED     = COAP_CODE(5,05)   /**< CoAP code 5.05, Decimal: 165, Hex: 0xA5. */
} coap_msg_code_t;

#endif // COAP_CODES_H__

/** @} */
