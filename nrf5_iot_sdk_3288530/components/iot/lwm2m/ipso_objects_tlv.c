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

#include "ipso_objects_tlv.h"
#include "lwm2m_tlv.h"

static void index_buffer_len_update(uint32_t * index, uint32_t * buffer_len, uint32_t max_buffer)
{
    *index      += *buffer_len;
    *buffer_len  = max_buffer - *index;
}

uint32_t ipso_tlv_ipso_digital_output_decode(ipso_digital_output_t * p_digital_output, 
                                             uint8_t *               p_buffer, 
                                             uint32_t                buffer_len)
{
    uint32_t    err_code;
    lwm2m_tlv_t tlv;
    
    uint32_t index = 0;

    while (index < buffer_len)
    {
        err_code = lwm2m_tlv_decode(&tlv, &index, p_buffer, buffer_len);
        
        if (err_code != NRF_SUCCESS) 
        {
            return err_code;
        }

        switch (tlv.id)
        {
            case IPSO_RR_ID_DIGITAL_OUTPUT_STATE:
            {
                p_digital_output->digital_output_state = tlv.value[0];
                break;
            }
            
            case IPSO_RR_ID_DIGITAL_OUTPUT_POLARITY:
            {
                p_digital_output->digital_output_polarity = tlv.value[0];
                break;
            }
            
            case IPSO_RR_ID_APPLICATION_TYPE:
            {
                p_digital_output->application_type.p_val = (char *)tlv.value;
                p_digital_output->application_type.len   = tlv.length;
                break;
            }
            
            default:
                break;
        }
    }

    return NRF_SUCCESS;
}
                                             
uint32_t ipso_tlv_ipso_digital_output_encode(uint8_t *               p_buffer, 
                                             uint32_t *              p_buffer_len, 
                                             ipso_digital_output_t * p_digital_output)
{
    uint32_t err_code;
    uint32_t max_buffer = *p_buffer_len;
    uint32_t index = 0;

    lwm2m_tlv_t tlv;
    tlv.id_type = TLV_TYPE_RESOURCE_VAL; // type is the same for all.
    
    // Encode state.
    lwm2m_tlv_bool_set(&tlv, p_digital_output->digital_output_state, IPSO_RR_ID_DIGITAL_OUTPUT_STATE);
    err_code = lwm2m_tlv_encode(p_buffer + index, p_buffer_len, &tlv);
    
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
        
    index_buffer_len_update(&index, p_buffer_len, max_buffer);

    // Encode polarity.
    lwm2m_tlv_bool_set(&tlv, p_digital_output->digital_output_polarity, IPSO_RR_ID_DIGITAL_OUTPUT_POLARITY);
    err_code = lwm2m_tlv_encode(p_buffer + index, p_buffer_len, &tlv);
    
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    index_buffer_len_update(&index, p_buffer_len, max_buffer);
    
    // Encode application type.
    lwm2m_tlv_string_set(&tlv, p_digital_output->application_type, IPSO_RR_ID_APPLICATION_TYPE);
    err_code = lwm2m_tlv_encode(p_buffer + index, p_buffer_len, &tlv);
    
    if (err_code != NRF_SUCCESS) 
    {
        return err_code;
    }
    
    index_buffer_len_update(&index, p_buffer_len, max_buffer);
    
    *p_buffer_len = index;
    
    return NRF_SUCCESS;
}
