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
 
/** @file coap_message.h
 *
 * @defgroup iot_sdk_coap_msg CoAP Message
 * @ingroup iot_sdk_coap
 * @{
 * @brief TODO.
 */

#ifndef COAP_MESSAGE_H__
#define COAP_MESSAGE_H__

#include <stdint.h>
#include "coap_api.h"
#include "coap_codes.h"
#include "coap_transport.h"
#include "coap_option.h"

#define COAP_PAYLOAD_MARKER       0xFF
#define ALIGN(a,b) (((a)+(b-1))&~(b-1))

/**@brief Create a new CoAP message.
 * 
 * @details The function will allocate memory for the message internally and return
 *          a CoAP message structure. The callback provided will be called if a matching 
 *          message ID/Token occurs in a response message.
 * @param[out] p_message     Pointer to set the generated coap_message_t structure to. 
 * @param[in]  p_init_config Initial configuration parameters of the message to generate.
 * 
 * @retval NRF_SUCCESS             If memory for the new message was allocated and the 
 *                                 initialization of the message went successfully.
 * @retval NRF_ERROR_NULL          Either the message or init_config parameter was NULL.
 * @retval NRF_ERROR_INVALID_PARAM If port number in the port field is set to 0.
 */
uint32_t coap_message_create(coap_message_t * p_message, coap_message_conf_t * p_init_config);

/**@brief Decode a message from a raw buffer.
 * 
 * @details When the underlying transport layer receives a message, it has to 
 *          be decoded into a CoAP message type structure. This functions returns
 *          a decoded message if decoding was successfully, or NULL otherwise.
 * 
 * @param[out] p_message     The generated coap_message_t after decoding the raw message.
 * @param[in]  p_raw_message Pointer to the encoded message memory buffer.
 * @param[in]  message_len   Length of the p_raw_message.
 *
 * @retval NRF_SUCCESS                  If the decoding of the message succeeds.
 * @retval NRF_ERROR_NULL               If pointer to the p_message or p_raw_message were NULL.
 * @retval NRF_ERROR_INVALID_LENGTH     If the message is less than 4 bytes, not containing a 
 *                                      full header.
 * @retval COAP_MESSAGE_INVALID_CONTENT If the message could not be decoded successfully. This
 *                                      could happen if message length provided is larger than 
 *                                      what is possible to decode (ex. missing payload marker).
 *
 */
uint32_t coap_message_decode(coap_message_t * p_message,
                             const uint8_t *  p_raw_message, 
                             uint16_t         message_len);

/**@brief Encode a CoAP message into a byte buffer.
 *
 * @details This functions has two operations. One is the actual encoding into a 
 *          byte buffer. The other is to query the size of a potential encoding. 
 *          If p_buffer variable is omitted, the return value will be the size of a 
 *          potential serialized message. This can be used to get some persistent memory from 
 *          transport layer. The message have to be kept until all potential 
 *          retransmissions has been attempted.
 *
 *          The p_message can be deleted after this point if the function succeeds.
 *
 * @param[in]    p_message Message to encode.
 * @param[in]    p_buffer  Pointer to the byte buffer where to put the encoded message.
 * @param[inout] p_length  Length of the provided byte buffer passed in by reference. 
 *                         If the value 0 is supplied, the encoding will not take place,
 *                         but only the dry run calculating the expected length of the 
 *                         encoded message.
 * 
 * @retval NRF_SUCCESS             If the encoding of the message succeeds.
 * @retval NRF_ERROR_NULL          If message or length parameter is NULL pointer. 
 * @retval NRF_ERROR_NO_MEM        If the provided buffer is not sufficient for 
 *                                 the encoded message.
 * @retval COAP_MESSAGE_ERROR_NULL If the message has indicated the length of data, 
 *                                 but memory pointer is NULL.
 */
uint32_t coap_message_encode(coap_message_t * p_message, 
                             uint8_t *        p_buffer, 
                             uint16_t *       p_length);

/**@brief Get the content format mask of the message.
 *
 * @param[in]  p_message Pointer to the message which to generate the content format mask from.
 *                       Should not be NULL.
 * @param[out] p_mask    Value by reference to the variable to fill the result mask into.
 * 
 * @retval NRF_SUCCESS         If the mask could be generated.
 * @retval NRF_ERROR_NULL      If the message pointer or the mask pointer given was NULL.
 */
uint32_t coap_message_ct_mask_get(coap_message_t * p_message, uint32_t * p_mask);

/**@brief Get the accept mask of the message.
 *
 * @param[in]  p_message Pointer to the message which to generate the accept mask from. 
 *                       Should not be NULL.
 * @param[out] p_mask    Value by reference to the variable to fill the result mask into.
 * 
 * @retval NRF_SUCCESS         If the mask could be generated.
 * @retval NRF_ERROR_NULL      If the message pointer or the mask pointer given was NULL.
 */
uint32_t coap_message_accept_mask_get(coap_message_t * p_message, uint32_t * p_mask);

#endif // COAP_MESSAGE_H__

/** @} */
