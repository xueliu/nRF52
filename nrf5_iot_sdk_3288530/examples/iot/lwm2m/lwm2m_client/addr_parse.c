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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "addr_parse.h"
#include "ipv6_parse.h"
#include "nrf_error.h"


static bool use_dtls_check(char * p_uri)
{
    if (p_uri[4] == 's')
    {
        return true;
    }
    
    return false;
}


uint32_t addr_parse_uri(uint8_t *  p_addr, 
                        uint16_t * p_port, 
                        bool *     p_use_dtls, 
                        char *     p_uri, 
                        uint8_t    uri_len)
{   
    
    uint16_t index = 0;
    
    if (uri_len < 4)
    {
        return NRF_ERROR_DATA_SIZE;
    }
    
    // Set default port for coap.
    *p_port = 5683; 
    
    bool use_dtls = use_dtls_check(p_uri);
    *p_use_dtls = use_dtls;
    
    // Check if it is secure coap.
    if (use_dtls == true)
    {
        // Skip coaps://[. 9 bytes in length.
        index = 9;
        
        // Set default port for coaps.
        *p_port = 5684; 
    }
    else 
    {
        // Skip coap://[. 8 bytes in length.
        index = 8;    
    }
    
    if (index >= uri_len)
    {
        return NRF_ERROR_INVALID_DATA;
    }
    
    uint8_t addr_len = 0;
    
    addr_len = strcspn(p_uri, "]") - index;
    
    // If we could not find any end bracket ']' or we read outside the length of the provided p_uri buffer.
    if ((addr_len == 0) || (addr_len + index > uri_len))
    {
        return NRF_ERROR_INVALID_DATA;
    }
    
    // Parse the IPv6 and put the result into result p_addr.
    uint32_t err_code = ipv6_parse_addr(p_addr, &p_uri[index], addr_len);
    if (err_code != NRF_SUCCESS)
    {
        return NRF_ERROR_INVALID_DATA;
    }
    
    // Increment the size of the IPv6 hostname.
    index += addr_len;

    // Increment the index to include the end bracket token ']'.
    index++;
    
    // Parse port   
    if (index < uri_len)
    {
        // Check if uri has port number defined. If so, retrieve it.
        char port_buf[6] = {'\0',};
        char * port_delimiter = strchr(&p_uri[index], ':');
        if (port_delimiter != NULL)
        {
            index++; // increase because of the recognized port delimiter (':').
            uint8_t port_index = 0;
            while ((index < uri_len) && (p_uri[index] != '/'))
            {
                port_buf[port_index] = p_uri[index];
                port_index++;
                index++;
            }
            *p_port = atoi(port_buf);
        }
    }
    
    return NRF_SUCCESS;
}
