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

/** @file coap_resource.h
 *
 * @defgroup iot_sdk_coap_resource CoAP Resource
 * @ingroup iot_sdk_coap
 * @{
 * @brief Private API of Nordic's CoAP Resource implementation.
 */

#ifndef COAP_RESOURCE_H__
#define COAP_RESOURCE_H__

#include <stdint.h>
#include "coap_api.h"
#include "sdk_config.h"
#include "coap_message.h"
#include "nrf_error.h"
#include "sdk_errors.h"

/**@brief Initialize the CoAP resource module.
 * 
 * @details This function will initialize the root element pointer to NULL.
 *          This way, a new root can be assigned registered. The first 
 *          resource added will be set as the new root.
 * 
 * @retval NRF_SUCCESS         This function will always return success.
 */
uint32_t coap_resource_init(void);

/**@brief Find a resource by traversing the resource names.
 *
 * @param[out] p_resource      Located resource.
 * @param[in]  pp_uri_pointers Array of strings which forms the hierarchical path to the resource.
 * @param[in]  num_of_uris     Number of URIs supplied through the path pointer list.
 *
 * @retval NRF_SUCCESS             The resource was instance located. 
 * @retval NRF_ERROR_NOT_FOUND     The resource was not located.
 * @retval NRF_ERROR_INVALID_STATE If no resource has been registered.
 */
uint32_t coap_resource_get(coap_resource_t ** p_resource, 
                           uint8_t **         pp_uri_pointers, 
                           uint8_t            num_of_uris);


/**@brief Process the request related to the resource. 
 * 
 * @details When a request is received and the resource has successfully been located it
 *          will pass on to this function. The method in the request will be matched against
 *          what the service provides of method handling callbacks. If the request expects a
 *          response this will be provided as output from this function. The memory provided 
 *          for the response must be provided from outside.
 *
 * @param[in]    p_resource Resource that will handle the request. 
 * @param[in]    p_request  The request to be handled.
 * @param[inout] p_response Response message which can be used by the resource populate 
 *                          the response message.
 */
uint32_t coap_resource_process_request(coap_resource_t * p_resource, 
                                       coap_message_t *  p_request, 
                                       coap_message_t *  p_response);

#endif // COAP_MESSAGE_H__

/** @} */
