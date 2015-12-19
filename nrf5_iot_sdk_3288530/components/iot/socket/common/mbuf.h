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
 
#ifndef MBUF_H__
#define MBUF_H__

struct mbuf {
    void        * p_ctx;
    struct mbuf * p_next;
};

typedef struct mbuf mbuf_t;
typedef uint8_t * (*mbuf_buf_fn)(mbuf_t * p_mbuf);
typedef uint32_t  (*mbuf_buf_len_fn)(mbuf_t * p_mbuf);
typedef void      (*mbuf_free_fn)(void * p_ctx);

typedef struct {
    mbuf_t              * p_current;
    mbuf_buf_fn           buf;
    mbuf_buf_len_fn       buf_len;
    mbuf_free_fn          free;
    uint32_t              readp_current;
} mbuf_head_t;


void     mbuf_init(mbuf_head_t * p_head, mbuf_buf_fn buf_fn, mbuf_buf_len_fn buf_len_fn, mbuf_free_fn free_fn);
uint32_t mbuf_alloc(mbuf_t ** pp_mbuf, void * p_ctx);

void     mbuf_write(mbuf_head_t * p_head, mbuf_t * p_mbuf);
uint32_t mbuf_read(mbuf_head_t * p_head, void * p_buf, uint32_t buf_size);

bool     mbuf_empty(mbuf_head_t * p_head);
uint32_t mbuf_size_total(mbuf_head_t * p_head);

#endif
