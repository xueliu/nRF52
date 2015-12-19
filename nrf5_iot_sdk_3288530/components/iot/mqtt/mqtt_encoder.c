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

/** @file mqtt_encoder.c
 *
 * @brief Encoding functions needed to create packet to be sent to the broker.
 */


#include "mqtt_internal.h"

#define MQTT_3_1_0_PROTO_DESC_LEN     6
#define MQTT_3_1_1_PROTO_DESC_LEN     4

const uint8_t mqtt_3_1_0_proto_desc_str[MQTT_3_1_0_PROTO_DESC_LEN] = {'M', 'Q', 'I', 's', 'd', 'p'};
const uint8_t mqtt_3_1_1_proto_desc_str[MQTT_3_1_1_PROTO_DESC_LEN] = {'M', 'Q', 'T', 'T'};

const mqtt_utf8_t mqtt_3_1_0_proto_desc =
{
    .p_utf_str  = (uint8_t *)&mqtt_3_1_0_proto_desc_str[0],
    .utf_strlen = MQTT_3_1_0_PROTO_DESC_LEN
};

const mqtt_utf8_t mqtt_3_1_1_proto_desc =
{
    .p_utf_str  = (uint8_t *)&mqtt_3_1_1_proto_desc_str[0],
    .utf_strlen = MQTT_3_1_1_PROTO_DESC_LEN
};

#ifdef MQTT_3_1_1
#define MQTT_CONNECT_PKT_QOS  0
#define MQTT_PROTOCOL_VERSION MQTT_VERSION_3_1_1_PROTO_LEVEL
const mqtt_utf8_t * p_mqtt_proto_desc = &mqtt_3_1_1_proto_desc;


#else // MQTT_3_1_1
#define MQTT_CONNECT_PKT_QOS  1
#define MQTT_PROTOCOL_VERSION MQTT_VERSION_3_1_0_PROTO_LEVEL
const mqtt_utf8_t * p_mqtt_proto_desc = &mqtt_3_1_0_proto_desc;

#endif //MQTT_3_1_1


uint32_t pack_uint8(uint8_t      val,
                    uint32_t     buffer_len,
                    uint8_t    * const buffer,
                    uint32_t   * const p_offset)
{
    uint32_t err_code = NRF_ERROR_DATA_SIZE;

    if (buffer_len > (*p_offset))
    {
        MQTT_TRC("[MQTT]: << pack_uint8 V:%02x BL:%08x, B:%p, O:%08x\r\n",
                  val, buffer_len, buffer, (*p_offset));

        // Pack value.
        buffer[(*p_offset)] = val;

        // Increment offset.
        (*p_offset) += SIZE_OF_UINT8;

        // Indicate success.
        err_code = NRF_SUCCESS;
    }

    return err_code;
}


uint32_t pack_uint16(uint16_t     val,
                     uint32_t     buffer_len,
                     uint8_t    * const buffer,
                     uint32_t   * const p_offset)
{
    uint32_t err_code = NRF_ERROR_DATA_SIZE;

    if (buffer_len > (*p_offset))
    {
        const uint32_t available_len = buffer_len - (*p_offset);

        MQTT_TRC("[MQTT]: << pack_uint16 V:%04x BL:%08x, B:%p, O:%08x A:%08x\r\n",
                  val, buffer_len, buffer, (*p_offset), available_len);

        if (available_len >= SIZE_OF_UINT16)
        {
            // Pack value.
            buffer[(*p_offset)]   = MSB_16(val);
            buffer[(*p_offset)+1] = LSB_16(val);

            // Increment offset.
            (*p_offset) += SIZE_OF_UINT16;

            // Indicate success.
            err_code = NRF_SUCCESS;
        }
    }

    return err_code;
}


uint32_t pack_utf8_str(mqtt_utf8_t const * const p_str,
                       uint32_t                  buffer_len,
                       uint8_t           * const buffer,
                       uint32_t          * const p_offset)
{
    uint32_t err_code = NRF_ERROR_DATA_SIZE;

    if (buffer_len > (*p_offset))
    {
        const uint32_t available_len = buffer_len - (*p_offset);
        err_code                     = NRF_ERROR_NO_MEM;

        MQTT_TRC("[MQTT]: << pack_utf8_str USL:%08x BL:%08x, B:%p, O:%08x A:%08x\r\n",
                 GET_UT8STR_BUFFER_SIZE(p_str), buffer_len, buffer, (*p_offset), available_len);

        if (available_len >= GET_UT8STR_BUFFER_SIZE(p_str))
        {
            // Length followed by string.
            err_code = pack_uint16(p_str->utf_strlen, buffer_len, buffer, p_offset);

            if (err_code == NRF_SUCCESS)
            {
                 memcpy(&buffer[(*p_offset)], p_str->p_utf_str, p_str->utf_strlen);

                 (*p_offset) += p_str->utf_strlen;

                 err_code = NRF_SUCCESS;
            }
        }
    }

    return err_code;
}

uint32_t pack_bin_str(mqtt_binstr_t const * const p_str,
                      uint32_t                    buffer_len,
                      uint8_t             * const buffer,
                      uint32_t            * const p_offset)
{
    uint32_t err_code = NRF_ERROR_DATA_SIZE;

    if (buffer_len > (*p_offset))
    {
        const uint32_t available_len = buffer_len - (*p_offset);
        err_code                     = NRF_ERROR_NO_MEM;

        MQTT_TRC("[MQTT]: << pack_bin_str BSL:%08x BL:%08x, B:%p, O:%08x A:%08x\r\n",
                 GET_BINSTR_BUFFER_SIZE(p_str), buffer_len, buffer, (*p_offset), available_len);

        if (available_len >= GET_BINSTR_BUFFER_SIZE(p_str))
        {
            memcpy(&buffer[(*p_offset)], p_str->p_bin_str, p_str->bin_strlen);

            (*p_offset) += p_str->bin_strlen;
            err_code = NRF_SUCCESS;
        }
    }

    return err_code;
}


void packet_length_encode(uint32_t remaining_length, uint8_t * p_buff, uint32_t * p_size)
{
    uint16_t         index  = 0;
    const uint32_t   offset = (*p_size);

    MQTT_TRC("[MQTT]: RL:0x%08x O:%08x P:%p\r\n", remaining_length, offset, p_buff);

    do
    {
        if (p_buff != NULL)
        {
            p_buff[offset+index] = remaining_length % 0x80;
        }

        remaining_length /= 0x80;

        if(remaining_length > 0)
        {
            if (p_buff != NULL)
            {
                p_buff[offset+index] |=  0x80;
            }
        }

        index++;

    } while (remaining_length > 0);

    MQTT_TRC("[MQTT]: RLS:0x%08x\r\n", index);

    *p_size += index;
}


uint32_t mqtt_encode_fixed_header(uint8_t message_type, uint32_t length, uint8_t ** pp_packet)
{
    uint32_t packet_length = 0xFFFFFFFF;

    if (MQTT_MAX_PAYLOAD_SIZE >= length)
    {
        uint32_t offset = 1;

        MQTT_TRC("[MQTT]: >> mqtt_encode_fixed_header MT:0x%02x L:0x%08x\r\n", message_type, length);
        packet_length_encode(length, NULL, &offset);

        MQTT_TRC("[MQTT]: Remaining length size = %02x\r\n", offset);

        uint8_t * p_mqtt_header = ((*pp_packet) - offset);

        // Reset offset.
        offset = 0;
        UNUSED_VARIABLE(pack_uint8(message_type, MQTT_MAX_PACKET_LENGTH, p_mqtt_header, &offset));
        packet_length_encode(length, p_mqtt_header, &offset);

        (* pp_packet) = p_mqtt_header;

        packet_length = (length + offset);
    }

    return packet_length;
}


uint32_t zero_len_str_encode(uint32_t            buffer_len,
                             uint8_t     * const buffer,
                             uint32_t    * const offset)
{
    return pack_uint16(0x0000, buffer_len, buffer, offset);
}


void connect_request_encode(const mqtt_client_t *  p_client,
                            uint8_t             ** pp_packet,
                            uint32_t            *  p_packet_length)
{
    uint32_t    err_code;
    uint32_t    offset        = 0;
    uint8_t   * p_payload     = &p_client->p_packet[MQTT_FIXED_HEADER_EXTENDED_SIZE];
    uint8_t     connect_flags = p_client->clean_session << 1; // Clean session always.

    memset(p_payload, 0, MQTT_MAX_PACKET_LENGTH);

    // Pack protocol description.
    MQTT_TRC("[MQTT]: Encoding Protocol Description. Str:%s Size:%08x.\r\n",
             p_mqtt_proto_desc->p_utf_str,
             p_mqtt_proto_desc->utf_strlen);

    err_code = pack_utf8_str(p_mqtt_proto_desc,
                             MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                             p_payload,
                             &offset);

    if(err_code == NRF_SUCCESS)
    {
        MQTT_TRC("[MQTT]: Encoding Protocol Version %02x.\r\n", MQTT_PROTOCOL_VERSION);
        // Pack protocol version.
        err_code = pack_uint8(MQTT_PROTOCOL_VERSION,
                              MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                              p_payload,
                              &offset);
    }

    // Remember position of connect flag and
    // leave one byte for it to be packed once we determine its value.
    const uint32_t connect_flag_offset = MQTT_FIXED_HEADER_EXTENDED_SIZE + offset;
    offset++;

    if(err_code == NRF_SUCCESS)
    {
        MQTT_TRC("[MQTT]: Encoding Keep Alive Time %04x.\r\n", MQTT_KEEPALIVE);
        // Pack keep alive time.
        err_code = pack_uint16(MQTT_KEEPALIVE,
                               MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                               p_payload,
                               &offset);
    }

    if(err_code == NRF_SUCCESS)
    {
        MQTT_TRC("[MQTT]: Encoding Client Id. Str:%s Size:%08x.\r\n",
                 p_client->client_id.p_utf_str,
                 p_client->client_id.utf_strlen);

        // Pack client id
        err_code = pack_utf8_str(&p_client->client_id,
                                 MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                 p_payload,
                                 &offset);
    }

    if(err_code == NRF_SUCCESS)
    {
        // Pack will topic and QoS
        if (p_client->p_will_topic != NULL)
        {
            MQTT_TRC("[MQTT]: Encoding Will Topic. Str:%s Size:%08x.\r\n",
                     p_client->p_will_topic->topic.p_utf_str,
                     p_client->p_will_topic->topic.utf_strlen);

            // Set Will topic in connect flags.
            connect_flags |= MQTT_CONNECT_FLAG_WILL_TOPIC;

            err_code = pack_utf8_str(&p_client->p_will_topic->topic,
                                      MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                      p_payload,
                                      &offset);

            if (err_code == NRF_SUCCESS)
            {
                // QoS is always 1 as of now.
                connect_flags |= ((p_client->p_will_topic->qos & 0x03) << 3);
                connect_flags |= p_client->will_retain << 5;

                if (p_client->p_will_message != NULL)
                {
                    MQTT_TRC("[MQTT]: Encoding Will Message. Str:%s Size:%08x.\r\n",
                             p_client->p_will_message->p_utf_str,
                             p_client->p_will_message->utf_strlen);

                    err_code = pack_utf8_str(p_client->p_will_message,
                                             MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                             p_payload,
                                             &offset);
                }
                else
                {
                    MQTT_TRC("[MQTT]: Encoding Zero Length Will Message.\r\n");
                    err_code = zero_len_str_encode(MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                                   p_payload,
                                                   &offset);
                }
            }
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        // Pack Username if any.
        if (p_client->p_user_name != NULL)
        {
            connect_flags |= MQTT_CONNECT_FLAG_USERNAME;

            MQTT_TRC("[MQTT]: Encoding Username. Str:%s, Size:%08x.\r\n",
                     p_client->p_user_name->p_utf_str,
                     p_client->p_user_name->utf_strlen);

            err_code = pack_utf8_str(p_client->p_user_name,
                                     MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                     p_payload,
                                     &offset);

            if (err_code == NRF_SUCCESS)
            {
                // Pack Password if any.
                if(p_client->p_password != NULL)
                {
                    MQTT_TRC("[MQTT]: Encoding Password. Str:%s Size:%08x.\r\n",
                             p_client->p_password->p_utf_str,
                             p_client->p_password->utf_strlen);

                     connect_flags |= MQTT_CONNECT_FLAG_PASSWORD;
                     err_code = pack_utf8_str(p_client->p_password,
                                              MQTT_MAX_VARIABLE_HEADER_N_PAYLOAD,
                                              p_payload,
                                              &offset);
                }
            }
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        // Pack the connect flags.
        p_client->p_packet[connect_flag_offset] = connect_flags;

        const uint8_t message_type = MQTT_MESSAGES_OPTIONS(MQTT_PKT_TYPE_CONNECT,
                                                           0, // Duplicate flag not set.
                                                           MQTT_CONNECT_PKT_QOS,
                                                           0); // Retain flag not set.

        offset = mqtt_encode_fixed_header(message_type,
                                          offset,
                                          &p_payload);

        (*p_packet_length) = offset;
        (*pp_packet)       = p_payload;
    }
    else
    {
        (*p_packet_length) = 0;
        (*pp_packet)       = NULL;
    }
}
