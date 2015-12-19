/* Copyright (c)  2015 Nordic Semiconductor. All Rights Reserved.
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

#include "iot_defines.h"
#include "iot_errors.h"
#include "sdk_config.h"
#include "mem_manager.h"
#include "mbuf.h"

static bool     mbuf_empty_entry(mbuf_head_t * p_head, mbuf_t * p_mbuf);

void mbuf_init(mbuf_head_t * p_head, mbuf_buf_fn buf_fn, mbuf_buf_len_fn buf_len_fn, mbuf_free_fn free_fn)
{
    p_head->p_current = NULL;
    p_head->readp_current = 0;
    p_head->buf = buf_fn;
    p_head->buf_len = buf_len_fn;
    p_head->free = free_fn;
}
 
uint32_t
mbuf_alloc(mbuf_t ** pp_mbuf, void * p_ctx)
{
    uint32_t mbuf_len = sizeof(mbuf_t);
    uint32_t err_code = nrf_mem_reserve((uint8_t **)pp_mbuf, &mbuf_len);
    if (err_code != NRF_SUCCESS)
    {
        *pp_mbuf = NULL;
    }
    else
    {
        mbuf_t * p_mbuf = *pp_mbuf;
        p_mbuf->p_ctx = p_ctx;
        p_mbuf->p_next = NULL;
    }
    return err_code;
}

static bool
mbuf_empty_entry(mbuf_head_t *p_head, mbuf_t * p_mbuf)
{
    return p_head->buf_len(p_mbuf) == p_head->readp_current;
}


void
mbuf_write(mbuf_head_t * p_head, mbuf_t * p_mbuf)
{
    if (p_head->p_current == NULL)
    {
        p_head->p_current = p_mbuf;
    }
    else
    {
        mbuf_t * p_current = p_head->p_current;
        while (p_current->p_next != NULL)
        {
            p_current = p_current->p_next;
        }
        p_current->p_next = p_mbuf;
    }
}

uint32_t
mbuf_read(mbuf_head_t * p_head, void * p_buf, uint32_t buf_size)
{
    mbuf_t * p_current = p_head->p_current;
    uint32_t nbytes = 0;
    while (nbytes < buf_size && p_current != NULL)
    {
        uint8_t * p_data = p_head->buf(p_current);
        uint32_t p_data_len = p_head->buf_len(p_current);

        uint32_t copy_len = MIN(buf_size, (p_data_len - p_head->readp_current));
        memcpy(p_buf, (void *)(p_data + p_head->readp_current), copy_len);
        p_head->readp_current += copy_len;
        nbytes += copy_len;
        if (mbuf_empty_entry(p_head, p_current) == true)
        {
            mbuf_t * p_next = p_current->p_next;
            p_head->free(p_current->p_ctx);
            (void) nrf_free((uint8_t *)p_current);
            p_current = p_next;
            p_head->readp_current = 0;
        }
    }
    p_head->p_current = p_current;
    return nbytes;
}

bool
mbuf_empty(mbuf_head_t * p_head)
{
    return p_head->p_current == NULL || mbuf_empty_entry(p_head, p_head->p_current);
}

uint32_t
mbuf_size_total(mbuf_head_t * p_head)
{
    uint32_t total_size= 0;
    mbuf_t * p_current = p_head->p_current;
    while (p_current != NULL)
    {
        total_size += p_head->buf_len(p_current);
        p_current = p_current->p_next;
    }
    return total_size;
}
