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

#ifndef ERRNO_H__
#define ERRNO_H__

#define EBADF            9
#define ENOMEM          12
#define EFAULT          14
#define EINVAL          22
#define EMFILE          24
#define EAGAIN          35
#define EPROTONOSUPPORT 43
#define EAFNOSUPPORT    47
#define EADDRINUSE      48
#define ENETDOWN        50
#define ENETUNREACH     51
#define ECONNRESET      54
#define EISCONN         56
#define ENOTCONN        57
#define ETIMEDOUT       60

int * __error(void);
void  set_errno(int);

#undef errno
#define errno (* __error())

#endif
