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

/** @file mqtt.c
 *
 * @brief MQTT Client API Implementation.
 */


#include "mqtt.h"
#include "mem_manager.h"
#include "mqtt_internal.h"
#include "iot_timer.h"

static mqtt_client_t      * m_mqtt_client[MQTT_MAX_CLIENTS];                             /**< MQTT Client table.*/
static const uint8_t        m_ping_packet[2] = {MQTT_PKT_TYPE_PINGREQ, 0x00};            /**< Never changing ping packet, needed for Keep Alive. */
SDK_MUTEX_DEFINE(m_mqtt_mutex)                                                           /**< Mutex variable for the module, currently unusued. */

const transport_procedure_t transport_fn[MQTT_TRASNPORT_MAX] =
{
    {
        mqtt_client_tcp_connect,
        mqtt_client_tcp_write,
        mqtt_client_tcp_read,
        mqtt_client_tcp_disconnect
    },
    {
        mqtt_client_tls_connect,
        mqtt_client_tls_write,
        mqtt_client_tls_read,
        mqtt_client_tls_disconnect
    }
};


static  uint32_t get_client_index(mqtt_client_t * const p_client)
{
    for (uint32_t index = 0; index < MQTT_MAX_CLIENTS; index++)
    {
        if (m_mqtt_client[index] == p_client)
        {
            return index;
        }
    }

    return MQTT_MAX_CLIENTS;
}


void client_init(mqtt_client_t * const p_client)
{
    MQTT_STATE_INIT(p_client);
    p_client->poll_abort_counter   = 0;
    p_client->p_password           = NULL;
    p_client->p_user_name          = NULL;
    p_client->p_will_topic         = NULL;
    p_client->p_will_message       = NULL;
    p_client->will_retain          = 0;
    p_client->clean_session        = 1;
    p_client->client_id.p_utf_str  = NULL;
    p_client->client_id.utf_strlen = 0;
    p_client->p_pending_packet     = NULL;
    p_client->pending_packetlen    = 0;
    p_client->p_security_settings  = NULL;

    // Free memory used for TX packets and reset the pointer.
    nrf_free(p_client->p_packet);
    p_client->p_packet = NULL;

    // Free TLS instance and reset the instance.
    UNUSED_VARIABLE(nrf_tls_free(&p_client->tls_instance));
    NRF_TLS_INTSANCE_INIT(&p_client->tls_instance);
}

void notify_disconnection(mqtt_client_t * p_client, uint32_t result)
{
    mqtt_evt_t        evt;
    mqtt_evt_cb_t     evt_cb = p_client->evt_cb;
    const uint32_t    client_index = get_client_index(p_client);

    // Remove the client from internal table.
    if (client_index != MQTT_MAX_CLIENTS)
    {
       m_mqtt_client[client_index] = NULL;
    }

    // Determine appropriate event to generate.
    if(MQTT_VERIFY_STATE(p_client, MQTT_STATE_CONNECTED))
    {
        evt.id     = MQTT_EVT_DISCONNECT;
        evt.result = result;
    }
    else
    {
        evt.id     = MQTT_EVT_CONNECT;
        evt.result = MQTT_CONNECTION_FAILED;
    }

    // Free the instance.
    client_init(p_client);

    // Notify application.

    if (evt_cb != NULL)
    {
        MQTT_MUTEX_UNLOCK();

        evt_cb(p_client, (const mqtt_evt_t *)&evt);

        MQTT_MUTEX_LOCK();
    }
}


uint32_t mqtt_init(void)
{
    SDK_MUTEX_INIT(m_mqtt_mutex);

    MQTT_MUTEX_LOCK();

    memset(m_mqtt_client, 0, sizeof(m_mqtt_client));

    MQTT_MUTEX_UNLOCK();

    return nrf_tls_init();
}


void mqtt_client_init(mqtt_client_t * const p_client)
{
    NULL_PARAM_CHECK_VOID(p_client);

    MQTT_MUTEX_LOCK();

    client_init(p_client);

    MQTT_MUTEX_UNLOCK();
}


uint32_t mqtt_connect(mqtt_client_t * const p_client)
{
    // Look for a free instance if available.
    uint32_t err_code     = NRF_SUCCESS;
    uint32_t client_index = 0;

    NULL_PARAM_CHECK(p_client);
    NULL_PARAM_CHECK(p_client->client_id.p_utf_str);

    MQTT_MUTEX_LOCK();

    for (client_index = 0; client_index < MQTT_MAX_CLIENTS; client_index++)
    {
         if (m_mqtt_client[client_index] == NULL)
         {
             // Found a free instance.
             m_mqtt_client[client_index] = p_client;

             // Allocate buffer packts in TX path.
             p_client->p_packet = nrf_malloc(MQTT_MAX_PACKET_LENGTH);
             break;
         }
    }

    if ((client_index == MQTT_MAX_CLIENTS) || (p_client->p_packet == NULL))
    {
        err_code = (NRF_ERROR_NO_MEM | IOT_MQTT_ERR_BASE);
    }
    else
    {
        err_code = tcp_request_connection(p_client);

        if(err_code != NRF_SUCCESS)
        {
            // Free the instance.
            m_mqtt_client[client_index] = NULL;
            nrf_free(p_client->p_packet);
            err_code = MQTT_ERR_TCP_PROC_FAILED;
        }
    }

    UNUSED_VARIABLE(p_client);

    MQTT_MUTEX_UNLOCK();

    return err_code;
}


uint32_t mqtt_publish(mqtt_client_t               * const p_client,
                      mqtt_publish_param_t  const * const p_param)
{
    uint32_t   err_code = MQTT_ERR_NOT_CONNECTED;
    uint32_t   offset   = 0;
    uint32_t   mqtt_packetlen = 0;
    uint8_t  * p_payload;
    
    NULL_PARAM_CHECK(p_client);
    NULL_PARAM_CHECK(p_param);

    MQTT_TRC("[MQTT]:[CID %p]:[State 0x%02x]: >> mqtt_publish Topic size 0x%08x, "
             "Data size 0x%08x\r\n",
             (p_client),
              p_client->state,
              p_param->message.topic.topic.utf_strlen,
              p_param->message.payload.bin_strlen);

    MQTT_MUTEX_LOCK();

    p_payload = &p_client->p_packet[MQTT_FIXED_HEADER_EXTENDED_SIZE];

    if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_PENDING_WRITE))
    {
        err_code = (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE);
    }
    else if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_CONNECTED))
    {
        memset(p_payload, 0, MQTT_MAX_PACKET_LENGTH);

        // Pack topic
        err_code = pack_utf8_str(&p_param->message.topic.topic,
                                 MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                 p_payload,
                                 &offset);

        if (err_code == NRF_SUCCESS)
        {
            if (p_param->message.topic.qos)
            {
                err_code = pack_uint16(p_param->message_id,
                                       MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                       p_payload,
                                       &offset);
            }
        }
        if (err_code == NRF_SUCCESS)
        {
            // Pack message on the topic.
            err_code = pack_bin_str(&p_param->message.payload,
                                    MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                    p_payload,
                                    &offset);
        }


        if (err_code == NRF_SUCCESS)
        {
            const uint8_t message_type = MQTT_MESSAGES_OPTIONS(MQTT_PKT_TYPE_PUBLISH,
                                                               0,  // Duplicate flag not set.
                                                               p_param->message.topic.qos,
                                                               0); // Retain flag not set.

            mqtt_packetlen = mqtt_encode_fixed_header(message_type, // Message type
                                                      offset,       // Payload size without the fixed header
                                                      &p_payload);    // Address where the p_payload is contained.


            //Publish message
            err_code = transport_fn[p_client->transport_type].write(p_client, p_payload, mqtt_packetlen);
        }
    }
    else
    {
        MQTT_TRC("[MQTT]: Packet allocation failed!\r\n");
        err_code = (NRF_ERROR_NO_MEM | IOT_MQTT_ERR_BASE);
    }

    MQTT_TRC("[MQTT]: << mqtt_publish\r\n");

    MQTT_MUTEX_UNLOCK();

    return err_code;
}

uint32_t mqtt_publish_ack(mqtt_client_t               * const p_client,
                          uint16_t                            message_id)
{
    uint32_t      err_code       = MQTT_ERR_NOT_CONNECTED;
    uint32_t      offset         = 0;
    uint32_t      mqtt_packetlen = 0;
    uint8_t     * p_payload;
    
    NULL_PARAM_CHECK(p_client);

    MQTT_TRC("[MQTT]:[CID %p]:[State 0x%02x]: >> mqtt_publish_ack Message id 0x%04x\r\n",
             (p_client),
              p_client->state,
              message_id);

    MQTT_MUTEX_LOCK();

    p_payload = &p_client->p_packet[MQTT_FIXED_HEADER_EXTENDED_SIZE];

    if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_PENDING_WRITE))
    {
        err_code = (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE);
    }
    else if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_CONNECTED))
    {
        memset(p_payload, 0, MQTT_MAX_PACKET_LENGTH);

        err_code = pack_uint16(message_id, MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD, p_payload, &offset);
        if (err_code == NRF_SUCCESS)
        {
            const uint8_t message_type = MQTT_MESSAGES_OPTIONS(MQTT_PKT_TYPE_PUBACK,
                                                               0,  // Duplicate flag not set.
                                                               0,  // QoS unused.
                                                               0); // Retain flag not set.

            mqtt_packetlen = mqtt_encode_fixed_header(message_type, // Message type
                                                      offset,       // Payload size without the fixed header
                                                      &p_payload);  // Address where the p_payload is contained.

            //Publish message
            err_code = transport_fn[p_client->transport_type].write(p_client,
                                                                    p_payload,
                                                                    mqtt_packetlen);
        }
    }

    MQTT_TRC("[MQTT]:[CID %p]:[State 0x%02x]: << mqtt_publish_ack result 0x%08x\r\n",
             (p_client),
              p_client->state,
              err_code);

    MQTT_MUTEX_UNLOCK();

    return err_code;
}


uint32_t mqtt_disconnect(mqtt_client_t * const p_client)
{
    uint32_t   err_code = MQTT_ERR_NOT_CONNECTED;
    
    NULL_PARAM_CHECK(p_client);

    MQTT_MUTEX_LOCK();

    err_code = transport_fn[p_client->transport_type].disconnect(p_client);

    notify_disconnection(p_client, err_code);

    MQTT_MUTEX_UNLOCK();

    return err_code;
}


uint32_t mqtt_subscribe(mqtt_client_t                  * const p_client,
                        mqtt_subscription_list_t const * const p_param)
{
    uint32_t      err_code       = MQTT_ERR_NOT_CONNECTED;
    uint32_t      offset         = 0;
    uint32_t      count          = 0;
    uint32_t      mqtt_packetlen = 0;
    uint8_t     * p_payload;
    
    NULL_PARAM_CHECK(p_client);
    NULL_PARAM_CHECK(p_param);

    MQTT_TRC("[MQTT]:[CID %p]:[State 0x%02x]: >> mqtt_subscribe message id 0x%04x "
             "topic count 0x%04x\r\n",
             (p_client),
              p_client->state,
              p_param->message_id,
              p_param->list_count);

    MQTT_MUTEX_LOCK();

    p_payload = &p_client->p_packet[MQTT_FIXED_HEADER_EXTENDED_SIZE];

    if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_PENDING_WRITE))
    {
        err_code = (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE);
    }
    else if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_CONNECTED))
    {
        memset(p_payload, 0, MQTT_MAX_PACKET_LENGTH);

        err_code = pack_uint16(p_param->message_id,
                               MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                               p_payload,
                               &offset);

        if (err_code == NRF_SUCCESS)
        {
            do
            {
                err_code = pack_utf8_str(&p_param->p_list[count].topic,
                                         MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                         p_payload,
                                         &offset);
                if (err_code == NRF_SUCCESS)
                {
                    err_code = pack_uint8(p_param->p_list[count].qos,
                                          MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                          p_payload,
                                          &offset);
                }
                count++;
            }while ((err_code != NRF_SUCCESS) || (count < p_param->list_count));
        }

        if (err_code == NRF_SUCCESS)
        {
            const uint8_t message_type = MQTT_MESSAGES_OPTIONS(MQTT_PKT_TYPE_SUBSCRIBE, 0, 1, 0);

            // Rewind the packet to encode the packet correctly
            mqtt_packetlen = mqtt_encode_fixed_header(message_type, // Message type, Duplicate Flag, QoS and retain flag setting.
                                                      offset,       // p_payload size without the fixed header
                                                      &p_payload);  // Address where the p_payload is contained. Header will encoded by rewinding the location.
            //Send message
            err_code = transport_fn[p_client->transport_type].write(p_client,
                                                                    p_payload,
                                                                    mqtt_packetlen);
        }
    }

    MQTT_TRC("[MQTT]:[CID %p]:[State 0x%02x]: << mqtt_subscribe result 0x%08x\r\n",
             (p_client),
              p_client->state,
              err_code);

    MQTT_MUTEX_UNLOCK();

    return err_code;
}


uint32_t mqtt_unsubscribe(mqtt_client_t                  * const p_client,
                          mqtt_subscription_list_t const * const p_param)
{
    uint32_t     err_code       = MQTT_ERR_NOT_CONNECTED;
    uint32_t     count          = 0;
    uint32_t     offset         = 0;
    uint32_t     mqtt_packetlen = 0;
    uint8_t    * p_payload;
    
    NULL_PARAM_CHECK(p_client);
    NULL_PARAM_CHECK(p_param);

    MQTT_MUTEX_LOCK();

    p_payload = &p_client->p_packet[MQTT_FIXED_HEADER_EXTENDED_SIZE];

    if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_PENDING_WRITE))
    {
        err_code = (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE);
    }
    else if (MQTT_VERIFY_STATE(p_client, MQTT_STATE_CONNECTED))
    {
        memset(p_payload, 0, MQTT_MAX_PACKET_LENGTH);

        err_code = pack_uint16(p_param->message_id,
                               MQTT_MAX_PACKET_LENGTH,
                               p_payload,
                               &offset);

        if (err_code == NRF_SUCCESS)
        {
            do
            {
                err_code = pack_utf8_str(&p_param->p_list[count].topic,
                                         MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                         p_payload,
                                         &offset);
                count++;
            }while ((err_code != NRF_SUCCESS) || (count < p_param->list_count));
        }

        if (err_code == NRF_SUCCESS)
        {
            const uint8_t message_type = MQTT_MESSAGES_OPTIONS(MQTT_PKT_TYPE_UNSUBSCRIBE,
                                                               0, // Duplicate flag.
                                                               MQTT_QoS_1_ATLEAST_ONCE,
                                                               0); // Retain flag.

            // Rewind the packet to encode the packet correctly
            mqtt_packetlen = mqtt_encode_fixed_header(message_type, // Message type, Duplicate Flag, QoS and retain flag setting.
                                                      offset,       // Payload size without the fixed header
                                                      &p_payload);  // Address where the p_payload is contained. Header will encoded by rewinding the location.

            //Send message
            err_code = transport_fn[p_client->transport_type].write(p_client,
                                                                    p_payload,
                                                                    mqtt_packetlen);
        }
    }

    MQTT_MUTEX_UNLOCK();

    return err_code;
}


uint32_t mqtt_ping(mqtt_client_t * const p_client)
{
    uint32_t err_code;

    NULL_PARAM_CHECK(p_client);
    
    MQTT_MUTEX_LOCK();

    if(MQTT_VERIFY_STATE(p_client, MQTT_STATE_PENDING_WRITE))
    {
        err_code = (NRF_ERROR_BUSY | IOT_MQTT_ERR_BASE);
    }
    else if(MQTT_VERIFY_STATE(p_client, MQTT_STATE_CONNECTED))
    {
        // Ping
        err_code = transport_fn[p_client->transport_type].write(p_client,
                                                                (uint8_t *)m_ping_packet,
                                                                 MQTT_PING_PKT_SIZE);
    }
    else
    {
        err_code = MQTT_ERR_NOT_CONNECTED;
    }

    MQTT_MUTEX_UNLOCK();

    return err_code;
}

uint32_t mqtt_abort(mqtt_client_t * const p_client)
{
    MQTT_MUTEX_LOCK();

    NULL_PARAM_CHECK(p_client);
    
    if (p_client->state != MQTT_STATE_IDLE)
    {
        mqtt_client_tcp_abort(p_client);
    }

    MQTT_MUTEX_UNLOCK();

    return NRF_SUCCESS;
}

uint32_t mqtt_live(void)
{
    iot_timer_time_in_ms_t elapsed_time;
    uint32_t index;

    // Note: The module should not be locked when calling this TLS API.
    nrf_tls_process();

    MQTT_MUTEX_LOCK();

    for (index = 0; index < MQTT_MAX_CLIENTS; index++)
    {
        mqtt_client_t * p_client = m_mqtt_client[index];
        if (p_client != NULL)
        {
            UNUSED_VARIABLE(iot_timer_wall_clock_delta_get(&p_client->last_activity,
                                                           &elapsed_time));

            if (elapsed_time > ((MQTT_KEEPALIVE - 2)* 1000))
            {
                UNUSED_VARIABLE(mqtt_ping(p_client));
            }
            if (p_client->p_pending_packet != NULL)
            {
                uint32_t err;
                err = transport_fn[p_client->transport_type].write(p_client,
                                                                   p_client->p_pending_packet,
                                                                   p_client->pending_packetlen);

                if (err == NRF_SUCCESS)
                {
                    p_client->p_pending_packet  = NULL;
                    p_client->pending_packetlen = 0;
                }
            }
        }
    }

    MQTT_MUTEX_UNLOCK();

    return NRF_SUCCESS;
}

