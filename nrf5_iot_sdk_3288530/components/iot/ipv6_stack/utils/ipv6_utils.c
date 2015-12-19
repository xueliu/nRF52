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
 
#include <stdint.h>
#include <stdbool.h>
#include "nordic_common.h"
#include "sdk_common.h"
#include "sdk_config.h"
#include "ipv6_utils.h"

void ipv6_checksum_calculate(const uint8_t * p_data, uint16_t len, uint16_t * p_checksum, bool flip_flag)
{
    uint16_t checksum_even = (((*p_checksum) & 0xFF00) >> 8);
    uint16_t checksum_odd  = ((*p_checksum) & 0x00FF);

    while (len)
    {
        if( len == 1 )
        {
          checksum_even += (*p_data);
          len           -= 1;
        }
        else
        {        
          checksum_even += *p_data++;
          checksum_odd  += *p_data++;
          len           -= 2;
        }
        
        if (checksum_odd & 0xFF00)
        {
            checksum_even += ((checksum_odd & 0xFF00) >> 8);
            checksum_odd   = (checksum_odd & 0x00FF);
        }

        if (checksum_even & 0xFF00)
        {
            checksum_odd += ((checksum_even & 0xFF00) >> 8);
            checksum_even = (checksum_even & 0x00FF);
        }
    }

    checksum_even = (checksum_even << 8) + (checksum_odd & 0xFFFF);

    if(flip_flag)
    {
        // We use 0xFFFF instead of 0x0000 because of not operator.
        if(checksum_even == 0xFFFF)
        {
            checksum_even = 0x0000;
        }
    }
    
    (*p_checksum) = (uint16_t)(checksum_even);
}

void ipv6_header_init(ipv6_header_t * p_ip_header)
{
    p_ip_header->version_traffic_class   = IPV6_DEFAULT_VER_TC;
    p_ip_header->traffic_class_flowlabel = IPV6_DEFAULT_TC_FL;
    p_ip_header->flowlabel               = IPV6_DEFAULT_FL;
    p_ip_header->next_header             = IPV6_NEXT_HEADER_RESERVED;
    p_ip_header->hoplimit                = IPV6_DEFAULT_HOP_LIMIT;
    p_ip_header->length                  = 0;
}
