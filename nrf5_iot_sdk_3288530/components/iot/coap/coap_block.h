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

 /** @file coap_block.h
 *
 * @defgroup iot_sdk_coap_block CoAP Block transfer
 * @ingroup iot_sdk_coap
 * @{
 * @brief CoAP block transfer options encoding and decoding interface and definitions.
 *
 */
 
#ifndef COAP_BLOCK_H__
#define COAP_BLOCK_H__

#include <stdint.h>

#define COAP_BLOCK_OPT_BLOCK_MORE_BIT_UNSET  0      /**< Value when more flag is set. */
#define COAP_BLOCK_OPT_BLOCK_MORE_BIT_SET    1      /**< Value when more flag is not set. */

typedef struct 
{
    uint8_t  more;                                  /**< More bit value. */
    uint16_t size;                                  /**< Size of the block in bytes. */
    uint32_t number;                                /**< Block number. */
} coap_block_opt_block1_t;

/**@brief Encode block1 option into its uint binary counter part.
 *
 * @param[out] p_encoded Encoded version of the coap block1 option value. Must not be NULL.
 * @param[in]  p_opt     Pointer to block1 option structure to be decoded into uint format. Must 
 *                       not be NULL.
 * 
 * @retval NRF_SUCCESS             If encoding of option was successful.
 * @retval NRF_ERROR_NULL          If one of the parameters supplied is a null pointer.
 * @retval NRF_ERROR_INVALID_PARAM If one of the fields in the option structure has an illegal 
 *                                 value.
 */
uint32_t coap_block_opt_block1_encode(uint32_t * p_encoded, coap_block_opt_block1_t * p_opt);

/**@brief Decode block1 option from a uint to its structure counter part.
 * 
 * @param[out] p_opt     Pointer to block1 option structure to be filled by the function. Must not
 *                       be NULL.
 * @param[in]  encoded   Encoded version of the coap block1 option value.
 *
 * @retval NRF_SUCCESS             If decoding of the option was successful.
 * @retval NRF_ERROR_NULL          If p_opt parameter is NULL.
 * @retval NRF_ERROR_INVALID_PARAM If the block number is higher then allowed by spec (more than 
                                   20 bits).
 * @retval NRF_ERROR_INVALID_DATA  If the size has the value of the reserved 2048 value (7).
 */
uint32_t coap_block_opt_block1_decode(coap_block_opt_block1_t * p_opt, uint32_t encoded);

#endif // COAP_BLOCK_H__

/** @} */
