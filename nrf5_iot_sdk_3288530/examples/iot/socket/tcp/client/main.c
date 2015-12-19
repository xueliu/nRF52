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

/** @file
 *
 * @defgroup iot_sdk_app_nrf_socket_tcp_client main.c
 * @{
 * @ingroup iot_sdk_app_nrf
 * @brief This file contains the source code for nRF TCP socket client.
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#define TCP_DATA_SIZE                 8

int main(void)
{
    struct sockaddr_in6 dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin6_family = AF_INET6;
    // Change this address to that of your server
    (void)inet_pton(AF_INET6, "2001:db8::1", &dest.sin6_addr);
    dest.sin6_port = htons(9000);

    int ret = -1;
    int s = -1;
    
    while (ret < 0)
    {
        s = socket(AF_INET6, SOCK_STREAM, 0);
        if (s < 0)
        {
            printf("[APPL]: Error creating socket.\r\n");
        }
        else
        {
            ret = connect(s, (struct sockaddr *)&dest, sizeof(dest));
            if (ret < 0)
            {
                // printf("[APPL]: Error connecting to server, errno %d\r\n", errno);
                (void) sleep(1);
                (void) close(s);
            }
        }
    }

    printf("[APPL]: Connected to server!\r\n");

    uint32_t seq_number = 0;
    while (seq_number < 100)
    {
        uint8_t tcp_data[TCP_DATA_SIZE];

        tcp_data[0] = (uint8_t )((seq_number >> 24) & 0x000000FF);
        tcp_data[1] = (uint8_t )((seq_number >> 16) & 0x000000FF);
        tcp_data[2] = (uint8_t )((seq_number >> 8)  & 0x000000FF);
        tcp_data[3] = (uint8_t )(seq_number         & 0x000000FF);

        tcp_data[4] = 'P';
        tcp_data[5] = 'i';
        tcp_data[6] = 'n';
        tcp_data[7] = 'g';

        errno = 0;
        ssize_t nbytes = send(s, tcp_data, TCP_DATA_SIZE, 0);
        if (nbytes != TCP_DATA_SIZE)
        {
            printf("[APPL]: Failed to send data, errno %d\r\n", errno);
        }
        else
        {
            printf("[APPL]: Data Tx, Sequence number 0x%08x\r\n", (unsigned int)seq_number);
        }

        (void) sleep(1);
        uint8_t rx_data[TCP_DATA_SIZE];
        errno = 0;
        nbytes = recv(s, rx_data, TCP_DATA_SIZE, 0);
        if (nbytes != TCP_DATA_SIZE)
        {
            printf("[APPL]: Error receiving data (%d bytes), errno %d\r\n", (int)nbytes, errno);
        }
        else
        {
            uint32_t rx_seq_number  = ((rx_data[0] << 24) & 0xFF000000);
            rx_seq_number |= ((rx_data[1] << 16) & 0x00FF0000);
            rx_seq_number |= ((rx_data[2] << 8)  & 0x0000FF00);
            rx_seq_number |= (rx_data[3]         & 0x000000FF);

            if (rx_seq_number != seq_number)
            {
                printf("[APPL]: Mismatch in sequence number.\r\n");
            }
            else
            {
                seq_number++;
            }
        }
    }
    (void) close(s);
    return 0;
}
