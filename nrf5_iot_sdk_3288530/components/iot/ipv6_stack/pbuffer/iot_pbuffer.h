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
 
 /** @file
 *
 * @defgroup iot_pbuffer Packet Buffer
 * @{
 * @ingroup iot_sdk_stack
 * @brief Packet buffer management for IPv6 stack layers to minimize data copy across stack layers.
 *
 * @details This module interfaces with the Memory Manager to allocate packet buffers
 *          for the IPv6 stack layers, without each layer having to ensure 
 *          sufficient header space for layers below.
 */
#ifndef IOT_PBUFFER__
#define IOT_PBUFFER__

#include <stdint.h>
#include <stdbool.h>

/**@brief IPv6 packet type identifiers that are needed to ensure that enough 
 * space is reserved for headers from layers below during memory allocation.
 */
typedef enum
{
    UNASSIGNED_TYPE        = 0,                                                          /**< Indicates that the packet buffer is unassigned and not in use. */
    RAW_PACKET_TYPE        = 1,                                                          /**< Raw packet, with no room made for headers of any lower layer. */
    IPV6_PACKET_TYPE       = 2,                                                          /**< Indicates that the packet buffer is requested for an entire IPv6 packet; pbuffer provisions 40 bytes of IPv6 header. */
    ICMP6_PACKET_TYPE      = 3,                                                          /**< Indicates that the packet buffer is requested for an ICMPv6 packet, and provision for 40 bytes of IPv6 header is made by pbuffer. */
    UDP6_PACKET_TYPE       = 4,                                                          /**< Indicates that the packet buffer is requested for a UDP packet, and provision for 40 bytes of IPv6 header and UDP header is made by pbuffer. */
    COAP_PACKET_TYPE       = 5                                                           /**< Indicates that the packet buffer is requested for a CoAP packet, and provision for 4 bytes of CoAP header, 8 bytes of UDP header, and 40 bytes of IPv6 header is made. */
}iot_pbuffer_type_t;

/**@brief Additional information that must be provided to the module during allocation
 * or reallocation to ensure optimal utilization of memory and avoid unnecessary data
 * copies.
 */
typedef enum
{
    PBUFFER_FLAG_DEFAULT             = 0,                                                /**< Default behavior with respect to memory allocation when allocating packet buffer. Memory will be allocated for the length indicated by this default.*/
    PBUFFER_FLAG_NO_MEM_ALLOCATION   = 1,                                                /**< Only allocate packet buffer, not memory. This is needed when a packet already exists and the packet buffer is needed only to feed it to the IPv6 stack.*/
}iot_pbuffer_flags_t;

/**@brief Packet buffer used for exchanging IPv6 payload across layers in both receive and transmit
 * paths.
 */
typedef struct
{
    iot_pbuffer_type_t     type;                                                         /**< Determines if any offset for lower layers must be provisioned for in the stack. */
    uint8_t              * p_memory;                                                     /**< Pointer to actual memory allocated for the buffer. */
    uint8_t              * p_payload;                                                    /**< Pointer to memory where the payload for the layer that allocates the packet buffer should be contained. */
    uint32_t               length;                                                       /**< Length of the payload of the layer processing it. This value can be modified by each layer of the IPv6 stack based on the required header information added by each.*/
}iot_pbuffer_t;


/**@brief Parameters required to allocate the packet buffer. */
typedef struct
{
    iot_pbuffer_type_t   type;                                                           /**< Payload type for which the packet buffer is requested to be allocated or reallocated. */
    iot_pbuffer_flags_t  flags;                                                          /**< Flags that indicate if memory allocation is needed or not. */
    uint32_t             length;                                                         /**< Length of payload for which the packet buffer is requested. */
}iot_pbuffer_alloc_param_t;


/**@brief Function for initializing the module.
 *
 * @retval NRF_SUCCESS If the module was successfully initialized. Otherwise, an error code that indicates the reason for the failure is returned.
 */
uint32_t iot_pbuffer_init(void);


/**@brief Function for allocating a packet buffer.
 *
 * @param[in] p_param    Pointer to allocation parameters that indicate the length of the payload requested,
 *                       the type of payload, and additional information using the flags. This
 *                       parameter cannot be NULL.
 * @param[out] pp_pbuffer Pointer to allocated packet buffer. This parameter shall
 *                       not be NULL.
 *
 * @retval NRF_SUCCESS If the packet buffer was successfully allocated. Otherwise, an error code that indicates the reason for the failure is returned.
 */
uint32_t iot_pbuffer_allocate(iot_pbuffer_alloc_param_t    *  p_param,
                              iot_pbuffer_t                ** pp_pbuffer);


/**@brief Function for reallocating a packet buffer.
 *
 * Reallocation requests are treated as follows:
 * - If the requested reallocation is less than or equal to the allocated size, 
 *   no data is moved, and the function returns NRF_SUCCESS.
 * - If the requested reallocation is more than what is allocated, the function 
 *   requests new memory, backs up existing data, and then frees the previously
 *   allocated memory.
 * - If reallocation is requested with the PBUFFER_FLAG_NO_MEM_ALLOCATION flag, 
 *   the function does not free previously allocated memory or copy it to the 
 *   new location. In this case, the application that uses the pbuffer must 
 *   decide when to move previously allocated memory and when to free it and 
 *   handle this.
 *
 * @param[in] p_param    Pointer to reallocation parameters that indicate the length of the payload requested,
 *                       the type of payload, and additional information using the flags. This
 *                       parameter cannot be NULL.
 * @param[in] p_pbuffer  Pointer to the packet buffer being reallocated. This parameter shall
 *                       not be NULL.
 *
 * @retval NRF_SUCCESS If the packet buffer was successfully reallocated. Otherwise, an error code that indicates the reason for the failure is returned.
 */
uint32_t iot_pbuffer_reallocate(iot_pbuffer_alloc_param_t  * p_param,
                                iot_pbuffer_t              * p_pbuffer);


/**@brief Function for freeing a packet buffer.
 *
 * This function frees the packet buffer. If the parameter free_flag is set, the
 * function tries to free the memory allocated as well. This action is performed
 * irrespective of whether the memory was allocated using the PBUFFER_FLAG_DEFAULT or
 *       the PBUFFER_FLAG_NO_MEM_ALLOCATION flag.
 *
 * @param[in] p_pbuffer  Pointer to the packet buffer requested to be freed. This parameter shall
 *                       not be NULL.
 * @param[in] free_flag  Indicates if the allocated memory should be freed or not when freeing the
 *                       packet buffer.
 *
 * @retval NRF_SUCCESS If the packet buffer was successfully freed. Otherwise, an error code that indicates the reason for the failure is returned.
 *
 */
uint32_t iot_pbuffer_free(iot_pbuffer_t  * p_pbuffer, bool free_flag);

#endif // IOT_PBUFFER__

/**@} */
