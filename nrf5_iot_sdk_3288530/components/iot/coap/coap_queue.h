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

/** @file coap_queue.h
 *
 * @defgroup iot_sdk_coap_queue CoAP Message Queue
 * @ingroup iot_sdk_coap
 * @{
 * @brief TODO.
 */

#ifndef COAP_QUEUE_H__
#define COAP_QUEUE_H__

#include <stdint.h>

#include "coap_transport.h"
#include "coap_message.h"

typedef struct 
{
    void *                   p_arg;                 /**< Miscellaneous pointer to application provided data that is associated with the message. Copied from the coap_message_t when creating the item. */
    uint32_t                 handle;                /**< Quick reference to the handle value of the current item. */
    uint16_t                 mid;                   /**< Message ID. */
    uint8_t                  token_len;             /**< Message Token length. */
    uint8_t                  token[8];              /**< Message Token value up to 8 bytes. */
    uint8_t                  retrans_count;         /**< Re-transmission attempt count. */
    uint16_t                 timeout;               /**< Time until new re-transmission attempt. */
    uint16_t                 timeout_val;           /**< Last timeout value used. */
    coap_port_t              port;                  /**< Source port to use when re-transmitting. */
    uint8_t *                p_buffer;              /**< Pointer to the data buffer containing the encoded CoAP message. */
    uint32_t                 buffer_len;            /**< Size of the data buffer containing the encoded CoAP message. */
    coap_remote_t            remote;                /**< Destination address and port number to the remote. */
    coap_response_callback_t callback;              /**< Callback function to be called upon response or transmission timeout. */
} coap_queue_item_t;

/**@brief Initilize the CoAP message queue. 
 * 
 * @retval NRF_SUCCESS If initialization completed successfully. 
 */
uint32_t coap_queue_init(void);

/**@brief Add item to the queue. 
 * 
 * @param[in] p_item      Pointer to an item which to add to the queue. The function will copy all
 *                        data provided. 
 * 
 * @retval NRF_SUCCESS         If adding the item was successful. 
 * @retval NRF_ERROR_NO_MEM    If max number of queued elements has been reached. This is 
 *                             configured by COAP_MESSAGE_QUEUE_SIZE in sdk_config.h. 
 * @retval NRF_ERROR_DATA_SIZE If the element could not be added. 
 */
uint32_t coap_queue_add(coap_queue_item_t * p_item);

/**@brief Remove item from the queue.
 *
 * @param[in] p_item      Pointer to an item which to remove from the queue. Should not be NULL.
 *
 * @retval NRF_SUCCESS         If the item was successfully removed from the queue.
 * @retval NRF_ERROR_NULL      If p_item pointer is NULL.
 * @retval NRF_ERROR_NOT_FOUND If the item was not located in the queue. 
 */
uint32_t coap_queue_remove(coap_queue_item_t * p_item);

/**@brief Search for item by token. 
 * 
 * @details Search the items for any item matching the token. 
 *
 * @param[out] pp_item    Pointer to be filled by the function if item matching the token 
 *                        has been found. Should not be NULL.
 * @param[in]  p_token    Pointer to token array to be matched. 
 * @param[in]  token_len  Length of the token to be matched.
 *
 * @retval NRF_SUCCESS         If an item was successfully located.
 * @retval NRF_ERROR_NULL      If pp_item pointer is NULL.
 * @retval NRF_ERROR_NOT_FOUND If no item was found. 
 */
uint32_t coap_queue_item_by_token_get(coap_queue_item_t ** pp_item, uint8_t * p_token, uint8_t token_len);

/**@brief Search for item by message id. 
 * 
 * @details Search the items for any item matching the message id.
 * 
 * @param[out] pp_item    Pointer to be filled by the funciton if item matching the message id 
 *                        has been found. Should not be NULL.
 * @param[in]  message_id Message id to be matched.
 * 
 * @retval NRF_SUCCESS If an item was successfully located.
 * @retval NRF_ERROR_NULL      If pp_item pointer is NULL.
 * @retval NRF_ERROR_NOT_FOUND If no item was found. 
 */
uint32_t coap_queue_item_by_mid_get(coap_queue_item_t ** pp_item, uint16_t message_id);

/**@brief Iterate through items.
 * 
 * @param[out] pp_item    Pointer to be filled by the search function upon finding the next 
 *                        observer starting from from the p_observer pointer provided. Should 
 *                        not be NULL. 
 * @param[in]  p_item     Pointer to the observer where to start the search.
 * 
 * @retval NRF_SUCCESS         If item was found.
 * @retval NRF_ERROR_NULL      If pp_item pointer is NULL.
 * @retval NRF_ERROR_NOT_FOUND If next item was not found. 
 */
uint32_t coap_queue_item_next_get(coap_queue_item_t ** pp_item, coap_queue_item_t * p_item);

#endif

/** @} */
