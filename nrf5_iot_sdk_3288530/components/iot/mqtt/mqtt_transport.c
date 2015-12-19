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

/** @file
 *
 * @brief MQTT Client Implementation over LwIP Stack port on nRF.
 *
 * This file contains the source code for MQTT Protocol over LwIP Stack for a nRF device.
 * The implementation is limited to MQTT Client role only.
 */


#include "mqtt_internal.h"

#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
/*lint -save -e607 */
#include "lwip/tcp.h"
/*lint -restore -e607 */


err_t tcp_write_complete_cb(void *p_arg, struct tcp_pcb *ttcp_id, u16_t len)
{
    MQTT_MUTEX_LOCK();

    mqtt_client_t   *p_client = (mqtt_client_t *)(p_arg);
    MQTT_RESET_STATE(p_client, MQTT_STATE_PENDING_WRITE);
    MQTT_TRC("[MQTT]:[%p]: TCP Write Complete.\r\n", p_client);

    MQTT_MUTEX_UNLOCK();

    return NRF_SUCCESS;
}


uint32_t mqtt_client_tcp_write(mqtt_client_t * p_client, uint8_t const * data, uint32_t datalen)
{
    uint32_t retval = (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE);

    if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_PENDING_WRITE))
    {
        retval = (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE);
    }
    else if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_TCP_CONNECTED))
    {
        tcp_sent((struct tcp_pcb *)p_client->tcp_id, tcp_write_complete_cb);

        MQTT_MUTEX_UNLOCK ();

        uint32_t err = tcp_write((struct tcp_pcb *)p_client->tcp_id,
                                 data,
                                 datalen,
                                 TCP_WRITE_FLAG_COPY);

        MQTT_MUTEX_LOCK ();

        if(err == ERR_OK)
        {
            MQTT_SET_STATE(p_client, MQTT_STATE_PENDING_WRITE);
            UNUSED_VARIABLE(iot_timer_wall_clock_get(&p_client->last_activity));
            MQTT_TRC("[MQTT]:[%p]: TCP Write in Progress, length 0x%08x.\r\n", p_client, datalen);
            retval = NRF_SUCCESS;
        }
        else
        {
            MQTT_TRC("[MQTT]: TCP write failed, err = %d\r\n", err);
            retval = (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE);
        }
    }

    return retval;
}


/**@breif Close TCP connection and clean up client instance. */
 void tcp_close_connection(const mqtt_client_t * p_client, uint32_t result)
{
    tcp_arg((struct tcp_pcb *)p_client->tcp_id, NULL);
    tcp_sent((struct tcp_pcb *)p_client->tcp_id, NULL);
    tcp_recv((struct tcp_pcb *)p_client->tcp_id, NULL);

    UNUSED_VARIABLE(tcp_close((struct tcp_pcb *)p_client->tcp_id));
}


/**@brief Handle MQTT data received on the transport (decrypted if on TLS). */
uint32_t mqtt_handle_rx_data(mqtt_client_t * p_client, uint8_t * p_data, uint32_t datalen)
{
    mqtt_evt_t       evt;
    mqtt_evt_cb_t    evt_cb   = NULL;
    uint32_t         err_code = NRF_SUCCESS;
    uint32_t         offset   = 1;
    uint32_t         remaining_length;

    switch(p_data[0] & 0xF0)
    {
        case MQTT_PKT_TYPE_PINGRSP:
        {
            MQTT_TRC("[MQTT]: Received PINGRSP!\r\n");
            break;
        }
        case MQTT_PKT_TYPE_PUBLISH:
        {
            evt.param.pub_message.dup_flag          = p_data[0] & MQTT_HEADER_DUP_MASK;
            evt.param.pub_message.retain_flag       = p_data[0] & MQTT_HEADER_RETAIN_MASK;
            evt.param.pub_message.message.topic.qos = ((p_data[0] & MQTT_HEADER_QOS_MASK) >> 1);

            MQTT_TRC("[MQTT]:[CID %p]: Received MQTT_PKT_TYPE_PUBLISH, QoS:%02x\r\n",
                     p_client, evt.param.pub_message.message.topic.qos);

            packet_length_decode(p_data, &remaining_length, &offset);

            if (err_code == NRF_SUCCESS)
            {
                err_code = unpack_utf8_str(&evt.param.pub_message.message.topic.topic,
                                           datalen,
                                           p_data,
                                           &offset);
            }

            if (evt.param.pub_message.message.topic.qos)
            {
                err_code = unpack_uint16(&evt.param.pub_message.message_id,
                                         datalen,
                                         p_data,
                                         &offset);
            }

            if (err_code == NRF_SUCCESS)
            {
                err_code = unpack_bin_str(&evt.param.pub_message.message.payload,
                                          datalen,
                                          p_data,
                                          &offset);

                // Zero length publish messages are permitted.
                if (err_code != NRF_SUCCESS)
                {
                     evt.param.pub_message.message.payload.p_bin_str = NULL;
                     evt.param.pub_message.message.payload.bin_strlen = 0;
                }
            }

            MQTT_TRC("[MQTT]: Received PUBLISH! %08x %08x\r\n",
                     evt.param.pub_message.message.payload.bin_strlen,
                     evt.param.pub_message.message.topic.topic.utf_strlen);

            evt.id       = MQTT_EVT_PUBLISH;
            evt.result   = err_code;

            UNUSED_VARIABLE(iot_timer_wall_clock_get(&p_client->last_activity));

            evt_cb = p_client->evt_cb;

            break;
        }
        case MQTT_PKT_TYPE_CONNACK:
        {
            MQTT_TRC("[MQTT]:[%p]: Received CONACK, MQTT connection up!\r\n", p_client);

            // Set state.
            MQTT_SET_STATE(p_client, MQTT_STATE_CONNECTED);

            evt.id        = MQTT_EVT_CONNECT;
            evt.result    = NRF_SUCCESS;

            // Set event notification callback to notify the event to application.
            evt_cb        = p_client->evt_cb;
            break;
        }
        case MQTT_PKT_TYPE_DISCONNECT:
        {
            MQTT_TRC("[MQTT]: Received DISCONNECT\r\n");

            tcp_close_connection(p_client, NRF_SUCCESS);

            break;
        }
        default:
        {
            break;
        }
    }

    if (evt_cb != NULL)
    {
        MQTT_MUTEX_UNLOCK();

        // Notify event to the application.
        evt_cb(p_client, (const mqtt_evt_t *)&evt);

        MQTT_MUTEX_LOCK();
    }

    return err_code;
}


uint32_t mqtt_client_tcp_read(mqtt_client_t * p_id, uint8_t * p_data, uint32_t datalen)
{
    return mqtt_handle_rx_data( p_id, p_data, datalen);
}


/**@brief Callback registered with TCP to handle incoming data on the connection. */
err_t recv_callback(void * p_arg, struct tcp_pcb * p_tcp_id, struct pbuf * p_buffer, err_t err)
{
    MQTT_MUTEX_LOCK();

    mqtt_client_t * p_client = (mqtt_client_t *)(p_arg);

    MQTT_TRC("[MQTT]: >> recv_callback, result 0x%08x, buffer %p\r\n", err, p_buffer);

    if (err == ERR_OK && p_buffer != NULL)
    {
        MQTT_TRC("[MQTT]: >> Packet buffer length 0x%08x \r\n", p_buffer->tot_len);
        tcp_recved(p_tcp_id, p_buffer->tot_len);
        UNUSED_VARIABLE(transport_fn[p_client->transport_type].read(p_client,
                                                                    p_buffer->payload,
                                                                    p_buffer->tot_len));
    }
    else
    {
        MQTT_TRC("[MQTT]: Error receiving data, closing connection\r\n");
        tcp_close_connection(p_client, MQTT_ERR_TRANSPORT_CLOSED);
        notify_disconnection(p_client, err);
    }

    UNUSED_VARIABLE(pbuf_free(p_buffer));

    MQTT_MUTEX_UNLOCK();

    return ERR_OK;
}


uint32_t mqtt_client_tcp_connect(mqtt_client_t * p_client)
{
    connect_request_encode(p_client, &p_client->p_pending_packet, &p_client->pending_packetlen);

    //Send MQTT identification message to broker.
    uint32_t err = transport_fn[p_client->transport_type].write(p_client,
                                                                p_client->p_pending_packet,
                                                                p_client->pending_packetlen);
    if (err != ERR_OK)
    {
        mqtt_client_tcp_abort(p_client);
    }
    else
    {
        p_client->p_pending_packet  = NULL;
        p_client->pending_packetlen = 0;
    }

    return err;
}


/**@brief TCP Connection Callback. MQTT Connection  */
err_t tcp_connection_callback(void * p_arg, struct tcp_pcb * p_tcp_id, err_t err)
{
    MQTT_MUTEX_LOCK();

    mqtt_client_t * p_client = (mqtt_client_t *)p_arg;

    if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_TCP_CONNECTING) &&
       (err == ERR_OK))
    {
        MQTT_SET_STATE(p_client, MQTT_STATE_TCP_CONNECTED);

        //Register callback.
        tcp_recv(p_tcp_id, recv_callback);
        uint32_t err_code = transport_fn[p_client->transport_type].connect(p_client);

        if (err_code != NRF_SUCCESS)
        {
            MQTT_TRC("[MQTT]: transport connect handler returned %08x\r\n", err_code);
            notify_disconnection(p_client, MQTT_CONNECTION_FAILED);
        }
    }

    MQTT_MUTEX_UNLOCK();

    return err;
}


void mqtt_client_tcp_abort(mqtt_client_t * p_client)
{
    tcp_abort((struct tcp_pcb *)p_client->tcp_id);
    notify_disconnection(p_client, MQTT_ERR_TCP_PROC_FAILED);
    MQTT_STATE_INIT(p_client);
}


void tcp_error_handler(void * p_arg, err_t err)
{
    MQTT_MUTEX_LOCK();

    mqtt_client_t * p_client = (mqtt_client_t *)(p_arg);
    notify_disconnection(p_client, err);

    MQTT_STATE_INIT(p_client);

    MQTT_MUTEX_UNLOCK();
}


err_t tcp_connection_poll(void * p_arg, struct tcp_pcb * p_tcp_id)
{
    MQTT_MUTEX_LOCK();

    mqtt_client_t * p_client = (mqtt_client_t *)(p_arg);

    p_client->poll_abort_counter++;

    MQTT_MUTEX_UNLOCK();

    return ERR_OK;
}


uint32_t tcp_request_connection(mqtt_client_t * p_client)
{
    p_client->poll_abort_counter = 0;
    p_client->tcp_id = (uint32_t)tcp_new_ip6();

    err_t err = tcp_connect_ip6((struct tcp_pcb *)p_client->tcp_id,
                                &p_client->broker_addr,
                                p_client->broker_port,
                                tcp_connection_callback);

    if(err != ERR_OK)
    {
        UNUSED_VARIABLE(mqtt_abort(p_client));
    }
    else
    {
        tcp_arg((struct tcp_pcb *)p_client->tcp_id, p_client);
        tcp_err((struct tcp_pcb *)p_client->tcp_id, tcp_error_handler);
        tcp_poll((struct tcp_pcb *)p_client->tcp_id, tcp_connection_poll, 10);
        tcp_accept((struct tcp_pcb *)p_client->tcp_id, tcp_connection_callback);

        MQTT_SET_STATE(p_client, MQTT_STATE_TCP_CONNECTING);
    }

    return err;
}


uint32_t mqtt_client_tcp_disconnect(mqtt_client_t * p_client)
{
    uint32_t err_code = NRF_ERROR_INVALID_STATE;

    if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_CONNECTED))
    {
        const uint8_t packet[] = {MQTT_PKT_TYPE_DISCONNECT, 0x00};
        UNUSED_VARIABLE(tcp_write((struct tcp_pcb *)p_client->tcp_id,
                       (void *)packet,
                        sizeof(packet),
                        1));

        tcp_close_connection(p_client, NRF_SUCCESS);
        err_code = NRF_SUCCESS;
    }
    else if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_TCP_CONNECTED))
    {
        tcp_close_connection(p_client, NRF_SUCCESS);
        err_code = NRF_SUCCESS;
    }

    return err_code;
}


uint32_t mqtt_client_tls_output_handler(nrf_tls_instance_t const * p_instance,
                                        uint8_t            const * p_data,
                                        uint32_t                   datalen)
{
    uint32_t          err_code = MQTT_ERR_WRITE;
    mqtt_client_t   * p_client = (mqtt_client_t   *)p_instance->transport_id;

    MQTT_MUTEX_LOCK ();

    MQTT_TRC("[MQTT]: << mqtt_client_tls_output_handler, client %p\r\n", p_client);

    if (p_client != NULL)
    {
        err_code = mqtt_client_tcp_write(p_client, p_data, datalen);
    }
     MQTT_TRC("[MQTT]: >> mqtt_client_tls_output_handler, client %p, result 0x%08x\r\n",
              p_client,
              err_code);

    MQTT_MUTEX_UNLOCK ();

    return err_code;
}


uint32_t mqtt_client_tls_connect(mqtt_client_t * p_client)
{
    const nrf_tls_options_t tls_option =
    {
        .output_fn      = mqtt_client_tls_output_handler,
        .transport_type = NRF_TLS_TYPE_STREAM,
        .role           = NRF_TLS_ROLE_CLIENT,
        .p_key_settings = p_client->p_security_settings
    };

    connect_request_encode(p_client,
                           &p_client->p_pending_packet,
                           &p_client->pending_packetlen);

    p_client->tls_instance.transport_id = (uint32_t)p_client;

    MQTT_MUTEX_UNLOCK ();

    uint32_t err_code = nrf_tls_alloc(&p_client->tls_instance, &tls_option);

    MQTT_MUTEX_LOCK ();

    return err_code;
}


uint32_t mqtt_client_tls_write(mqtt_client_t * p_client,
                               uint8_t const * p_data,
                               uint32_t        datalen)
{
    MQTT_MUTEX_UNLOCK ();

    //MQTT_TRC("[MQTT]: << mqtt_client_tls_write\r\n");
    uint32_t err_code = nrf_tls_write(&p_client->tls_instance, p_data, &datalen);

    MQTT_MUTEX_LOCK ();

    return err_code;
}


uint32_t mqtt_client_tls_read(mqtt_client_t * p_client, uint8_t * p_data, uint32_t datalen)
{
    //MQTT_TRC("[MQTT]: << mqtt_client_tls_read\r\n");

    uint32_t err = nrf_tls_input(&p_client->tls_instance, p_data, datalen);

    if ((err == NRF_SUCCESS) && (p_client->p_pending_packet == NULL))
    {
        uint32_t   rx_datalen = 1024;
        uint8_t  * p_data     = nrf_malloc(1024);

        MQTT_MUTEX_UNLOCK ();

        if (p_data != NULL)
        {
            err = nrf_tls_read(&p_client->tls_instance,
                               p_data,
                               &rx_datalen);

            MQTT_MUTEX_LOCK ();

            if ((err == NRF_SUCCESS) && (rx_datalen > 0))
            {
                 err = mqtt_handle_rx_data(p_client, p_data, rx_datalen);
            }

            nrf_free(p_data);
        }
    }

    return err;
}


uint32_t mqtt_client_tls_disconnect(mqtt_client_t * p_client)
{
    return mqtt_client_tcp_disconnect(p_client);
}
