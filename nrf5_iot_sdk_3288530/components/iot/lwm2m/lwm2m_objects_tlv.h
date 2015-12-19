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

/**@file lwm2m_objects_tlv.h
 *
 * @defgroup iot_sdk_lwm2m_objects_tlv OMA LWM2M object TLV encoder and decoder API
 * @ingroup iot_sdk_lwm2m
 * @{
 * @brief OMA LWM2M object TLV encoder and decoder API.
 */ 

#ifndef LWM2M_OBJECTS_TLV_H__
#define LWM2M_OBJECTS_TLV_H__
 
#include <stdint.h>
#include "lwm2m_objects.h"

/**@brief Decode a LWM2M server object from a TLV byte buffer. 
 * 
 * @note    Resource values NOT found in the tlv will not be altered.
 *
 * @warning lwm2m_string_t and lwm2m_opaque_t values will point to the byte buffer and needs 
 *          to be copied by the application before the byte buffer is freed.
 *
 * @param[out] p_server   Pointer to a LWM2M server object to be filled by the decoded TLVs.
 * @param[in]  p_buffer   Pointer to the TLV byte buffer to be decoded.
 * @param[in]  buffer_len Size of the buffer to be decoded.
 *
 * @retval NRF_SUCCESS If decoding was successfull.
 */
uint32_t lwm2m_tlv_server_decode(lwm2m_server_t * p_server, 
                                 uint8_t *        p_buffer, 
                                 uint32_t         buffer_len);

/**@brief Encode a LWM2M server object to a TLV byte buffer.
 * 
 * @param[out]   p_buffer     Pointer to a byte buffer to be used to fill the encoded TLVs.
 * @param[inout] p_buffer_len Value by reference indicating the size of the buffer provided. 
 *                            Will return the number of used bytes on return.
 * @param[in]    p_server     Pointer to the LWM2M server object to be encoded into TLVs.
 *
 * @retval NRF_SUCCESS If the encoded was successfull.
 */
uint32_t lwm2m_tlv_server_encode(uint8_t *        p_buffer, 
                                 uint32_t *       p_buffer_len, 
                                 lwm2m_server_t * p_server);

/**@brief Decode a LWM2M security object from a TLV byte buffer. 
 *
 * @note    Resource values NOT found in the tlv will not be altered.
 * 
 * @warning lwm2m_string_t and lwm2m_opaque_t values will point to the byte buffer and needs 
 *          to be copied by the application before the byte buffer is freed.
 *
 * @param[out] p_security Pointer to a LWM2M server object to be filled by the decoded TLVs.
 * @param[in]  p_buffer   Pointer to the TLV byte buffer to be decoded.
 * @param[in]  buffer_len Size of the buffer to be decoded.
 *
 * @retval NRF_SUCCESS If decoding was successfull.
 */
uint32_t lwm2m_tlv_security_decode(lwm2m_security_t * p_security, 
                                   uint8_t *          p_buffer, 
                                   uint32_t           buffer_len);

/**@brief Encode a LWM2M security object to a TLV byte buffer.
 * 
 * @param[out]   p_buffer     Pointer to a byte buffer to be used to fill the encoded TLVs.
 * @param[inout] p_buffer_len Value by reference inicating the size of the buffer provided. 
 *                            Will return the number of used bytes on return.
 * @param[in]    p_security   Pointer to the LWM2M security object to be encoded into TLVs.
 *
 * @retval NRF_SUCCESS If the encoded was successfull.
 */
uint32_t lwm2m_tlv_security_encode(uint8_t *          p_buffer, 
                                   uint32_t *         p_buffer_len, 
                                   lwm2m_security_t * p_security);

#endif // LWM2M_OBJECTS_TLV_H__

/** @} */
