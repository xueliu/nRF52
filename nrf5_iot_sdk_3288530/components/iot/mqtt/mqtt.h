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

/** @file mqtt.h
 *
 * @defgroup iot_sdk_mqtt_api MQTT Client on nRF5x
 * @ingroup iot_sdk_mqtt
 * @{
 * @brief MQTT Client Implementation on the Nordic nRF platforms.
 *
 * @details
 * MQTT Client's Application interface is defined in this header.
 *
 * @note The implementation assumes LwIP Stack is available with TCP module enabled.
 *
 * @note By default the implementation uses MQTT version 3.1.0. However few cloud services
 *       like the Xively use the version 3.1.1. For this please define MQTT_3_1_1 and recompile the
 *       example.
 */

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include "iot_defines.h"
#include "iot_timer.h"
#include "nrf_tls.h"


/**@brief MQTT Asynchronous Events notified to the application from the module
 *        through the callback registered by the application. */
typedef enum
{
    MQTT_EVT_CONNECT,                                                          /**< Connection Event. Event result accompanying the event indicates whether the connection failed or succeeded. */
    MQTT_EVT_DISCONNECT,                                                       /**< Disconnection Event. MQTT Client Reference is no longer valid once this event is received for the client. */
    MQTT_EVT_PUBLISH,                                                          /**< Publish event received when message is published on a topic client is subscribed to. */
    MQTT_EVT_PUBLISH_ACK,                                                      /**< Acknowledgement for published message with QoS 1. */
    MQTT_EVT_PUBLISH_REC,                                                      /**< Reception confirmation for published message with QoS 2. */
    MQTT_EVT_PUBLISH_REL,                                                      /**< Release of published published messages with QoS 2. */
    MQTT_EVT_PUBLISH_COMP,                                                     /**< Confirmation to a publish release message. Applicable only to QoS 2 messages. */
    MQTT_EVT_SUBSCRIBE_ACK,                                                    /**< Acknowledgement to a subscription request. */
    MQTT_EVT_UNSUBSCRIBE_ACK                                                   /**< Acknowledgement to a unsubscription request. */
} mqtt_evt_id_t;

/**@brief MQTT transport type. */
typedef enum
{
    MQTT_TRANSPORT_NON_SECURE = 0x00,                                          /**< Use non secure TCP transport for MQTT connection. */
    MQTT_TRANSPORT_SECURE     = 0x01,                                          /**< Use secure TCP transport (TLS) for MQTT connection. */
    MQTT_TRASNPORT_MAX        = 0x02                                           /**< Shall not be used as a transport type. Indicator of maximum transport types possible. */
} mqtt_transport_type_t;

/**@brief MQTT Quality of Service types. */
typedef enum
{
    MQTT_QoS_0_AT_MOST_ONCE  = 0x00,                                           /**< Lowest Quality of Service, no acknowledgement needed for published message. */
    MQTT_QoS_1_ATLEAST_ONCE  = 0x01,                                           /**< Medium Quality of Service, if acknowledgement expected for published message, duplicate messages permitted. */
    MQTT_QoS_2_EACTLY_ONCE   = 0x02                                            /**< Highest Quality of Service, acknowledgement expected and message shall be published only once. Message not published to interested parties unless client issues a PUBREL. */
} mqtt_qos_t;

/**@brief MQTT client forward declaration @ref mqtt_client_t for details. */
typedef struct mqtt_client_t mqtt_client_t;

/**@brief Abstracts UTF-8 encoded strings. */
typedef struct
{
    uint8_t    * p_utf_str;                                                    /**< Pointer to UTF-8 string. */
    uint32_t     utf_strlen;                                                   /**< Length of UTF string. */
} mqtt_utf8_t;

/**@brief Abstracts binary strings. */
typedef struct
{
    uint8_t    * p_bin_str;                                                    /**< Pointer to binary stream. */
    uint32_t     bin_strlen;                                                   /**< Length of binary stream. */
} mqtt_binstr_t;

/**@brief Abstracts MQTT UTF-8 encoded topic that can be subscribed to or published. */
typedef struct
{
    mqtt_utf8_t     topic;                                                     /**< Topic on to be published or subscribed to. */
    uint8_t         qos;                                                       /**< Quality of service requested for the subscription. @ref mqtt_qos_t for details. */
} mqtt_topic_t;

/**@brief Abstracts MQTT UTF-8 encoded unique client identifier. */
typedef mqtt_utf8_t mqtt_client_id_t;

/**@brief Abstracts MQTT UTF-8 encoded password to be used for the client connection. */
typedef mqtt_utf8_t mqtt_password_t;

/**@brief Abstracts MQTT UTF-8 encoded username to be used for the client connection. */
typedef mqtt_utf8_t mqtt_username_t;

/**@brief Abstracts will message used in @ref mqtt_connect request.
 *
 * @note utf8 is used here instead of binary string as a zero length encoding is expected in
 *       will message is empty.
 */
typedef mqtt_utf8_t mqtt_will_message_t;

/**@brief Abstracts message in binary encoded string received or published on a topic. */
typedef mqtt_binstr_t mqtt_message_t;

/**@brief Parameters for a publish message. */
typedef struct
{
    mqtt_topic_t    topic;                                                     /**< Topic on which data was published. */
    mqtt_message_t  payload;                                                   /**< Payload on the topic published. */
} mqtt_publish_message_t;

typedef struct
{
    mqtt_publish_message_t   message;                                          /**< Messages including topic, QoS and its payload (if any) to be published. */
    uint16_t                 message_id;                                       /**<  Message id used for the publish message. Redundant for QoS 0. */
    uint8_t                  dup_flag:1;                                       /**< Duplicate flag. If 1, it indicates the message is being retransmitted. Has no meaning with QoS 0. */
    uint8_t                  retain_flag:1;                                    /**< retain flag. If 1, the message shall be stored persistently by the broker. */
} mqtt_publish_param_t;

/**@brief List of topics in a subscription request. */
typedef struct
{
    mqtt_topic_t    *       p_list;                                            /**< Array containing topics along with QoS for each. */
    uint32_t                list_count;                                        /**< Number of topics in the subscription list */
    uint16_t                message_id;                                        /**< Message id used to identify subscription request. */
} mqtt_subscription_list_t;

/**
 * @brief Defines event parameters notified along with asynchronous events to the application.
 *        Currently, only MQTT_EVT_PUBLISH is accompanied with parameters.
 */
typedef union
{
    mqtt_publish_param_t  pub_message;                                          /**< Parameters accompanying MQTT_EVT_PUBLISH event. */
} mqtt_evt_param_t;

/**@brief Defined MQTT asynchronous event notified to the application. */
typedef struct
{
    mqtt_evt_id_t         id;                                                  /**< Identifies the event. */
    mqtt_evt_param_t      param;                                               /**< Contains parameters (if any) accompanying the event. */
    uint32_t              result;                                              /**< Event result. For example, MQTT_EVT_CONNECT has a result code indicating success or failure code of connection procedure. */
} mqtt_evt_t;

/**@brief Asynchronous event notification callback registered by the application with
 *        the module to receive module events.
 *
 * @param[in] p_client Identifies the client for which the event is notified.
 * @param[in] p_evet   Event description along with result and associated parameters (if any).
 */
typedef void (*mqtt_evt_cb_t)(mqtt_client_t * const p_client, const mqtt_evt_t * p_evt);

/**@brief MQTT Client definition to maintain information relevant to the client. */
struct mqtt_client_t
{
    mqtt_client_id_t         client_id;                                        /**< Unique client identification to be used for the connection. Shall be zero length or NULL valued. */
    mqtt_username_t        * p_user_name;                                      /**< User name (if any) to be used for the connection. NULL indicates no user name. */
    mqtt_password_t        * p_password;                                       /**< Password (if any) to be used for the connection. Note that if password is provided, user name shall also be provided. NULL indicates no password. */
    mqtt_topic_t           * p_will_topic;                                     /**< Will topic and QoS. Can be NULL. */
    mqtt_will_message_t    * p_will_message;                                   /**< Will message. Can be NULL. Non NULL value valid only if will topic is not NULL. */
    nrf_tls_key_settings_t * p_security_settings;                              /**< Provide security settings like PSK, own certificate etc here. The memory provided for the settings shall be resident. */
    mqtt_evt_cb_t            evt_cb;                                           /**< Application callback registered with the module to get MQTT events. */
    ipv6_addr_t              broker_addr;                                      /**< IPv6 Address of MQTT broker to which client connection is requested. */
    uint16_t                 broker_port;                                      /**< Broker's Port number. */
    uint8_t                  poll_abort_counter;                               /**< Poll abort counter maintained for the TCP connection. */
    uint8_t                  transport_type;                                   /**< Transport type selection for client instance. @ref mqtt_transport_type_t for possible values. MQTT_TRASNPORT_MAX is not a valid type.*/
    uint8_t                  will_retain:1;                                    /**< Will retain flag, 1 if will message shall be retained persistently. */
    uint8_t                  clean_session:1;                                  /**< Clean session flag indicating a fresh (1) or a retained session (0). Default is 1. */
    iot_timer_time_in_ms_t   last_activity;                                    /**< Internal. Ticks maintaining wallcock in last activity that occurred. Needed for periodic PING. */
    uint32_t                 state;                                            /**< Internal. Shall not be touched by the application. Client's state in the connection. */
    uint32_t                 tcp_id;                                           /**< Internal. Shall not be touched by the application. TCP Connection Reference provided by the IP stack. */
    uint8_t                * p_packet;                                         /**< Internal. Shall not be touched by the application. Used for creating MQTT packet in TX path. */
    uint8_t                * p_pending_packet;                                 /**< Internal. Shall not be touched by the application. */
    nrf_tls_instance_t       tls_instance;                                     /**< Internal. Shall not be touched by the application. TLS instance identifier. Valid only if transport is a secure one. */
    uint32_t                 pending_packetlen;                                /**< Internal. Shall not be touched by the application. */
};


/**
 * @brief Initializes the module.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 *
 * @note Shall be called before initiating any procedures on the module.
 * @note If module initialization fails, no module APIs shall be called.
 */
uint32_t mqtt_init(void);


/**
 * @brief Initializes the client instance.
 *
 * @param[in] p_client Client instance for which the procedure is requested.
 *                     Shall not be NULL.
 *
 * @note Shall be called before connecting the client in order to avoid unexpected behaviour
 *       caused by unitialized parameters.
 */
void mqtt_client_init(mqtt_client_t * const p_client);


/**
 * @brief API to request new MQTT client connection.
 *
 * @param[out] p_client  Client instance for which the procedure is requested.
 *                       Shall not be NULL.
 *
 * @note This memory is assumed to be resident until mqtt_disconnect is called.
 * @note Any subsequent changes to parameters like broker address, user name, device id, etc. have
 *       no effect once MQTT connection is established.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 *
 * @note Default protocol revision used for connection request is 3.1.0. Please define MQTT_3_1_1
 *       to use protocol 3.1.1.
 * @note If more than one simultaneous client connections are needed, please define
 *        MQTT_MAX_CLIENTS to override default of 1.
 * @note Please define MQTT_KEEPALIVE time to override default of 1 minute.
 * @note Please define MQTT_MAX_PACKET_LENGTH time to override default of 128 bytes.
 *       Ensure the system has enough memory for the new length per client.
 */
uint32_t mqtt_connect(mqtt_client_t * const p_client);


/**
 * @brief API to publish messages on topics.
 *
 * @param[in]  p_client   Client instance for which the procedure is requested.
 *                        Shall not be NULL.
 * @param[in]  p_param    Parameters to be used for the publish message.
 *                        Shall not be NULL.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 *
 * @note Default protocol revision used for connection request is 3.1.0.
 *       Please define MQTT_3_1_1 to use protocol 3.1.1.
 */
uint32_t mqtt_publish(mqtt_client_t               * const p_client,
                      mqtt_publish_param_t  const * const p_param);


/**
 * @brief API used by subscribing client to send acknowledgement to the broker.
 *        Applicable only to QoS 1 publish messages.
 *
 * @param[in]  p_client   Client instance for which the procedure is requested.
 *                        Shall not be NULL.
 * @param[in]  message_id Identifies message being acknowledged.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 *
 * @note Default protocol revision used for connection request is 3.1.0.
 *       Please define MQTT_3_1_1 to use protocol 3.1.1.
 */
uint32_t mqtt_publish_ack(mqtt_client_t               * const p_client,
                          uint16_t                            message_id);


/**
 * @brief API to send assured acknowledgement from a subscribing client to the broker.
 *        Should be called on reception of @ref MQTT_EVT_PUBLISH with QoS set to
 *        @ref MQTT_QoS_2_EACTLY_ONCE.
 *
 * @param[in]  p_client   Identifies client instance for which the procedure is requested.
 *                        Shall not be NULL.
 * @param[in]  message_id Identifies message being acknowledged.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 *
 * @note Default protocol revision used for connection request is 3.1.0.
 *       Please define MQTT_3_1_1 to use protocol 3.1.1.
 */
uint32_t mqtt_publish_receive(mqtt_client_t               * const p_client,
                              uint16_t                            message_id);


/**
 * @brief API to used by publishing client to request releasing published data.
 *        Shall be used only after @ref MQTT_EVT_PUBLISH_REC is received and is valid
 *        only for QoS level @ref MQTT_QoS_2_EACTLY_ONCE.
 *
 * @param[in]  p_client   Client instance for which the procedure is requested.
 *                        Shall not be NULL.
 * @param[in]  message_id Identifies message being released.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 *
 * @note Default protocol revision used for connection request is 3.1.0.
 *       Please define MQTT_3_1_1 to use protocol 3.1.1.
 */
uint32_t mqtt_publish_release(mqtt_client_t               * const p_client,
                              uint16_t                            message_id);


/**
 * @brief API  used by subscribing clients to acknowledge reception of a released message.
 *        Should be used on reception @ref MQTT_EVT_PUBLISH_REL event.
 *
 * @param[in]  p_client   Identifies client instance for which the procedure is requested.
 *                        Shall not be NULL.
 * @param[in]  message_id Identifies message being completed.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 *
 * @note Default protocol revision used for connection request is 3.1.0.
 *       Please define MQTT_3_1_1 to use protocol 3.1.1.
 */
uint32_t mqtt_publish_complete(mqtt_client_t               * const p_client,
                               uint16_t                            message_id);


/**
 * @brief API to request subscribing to a topic on the connection.
 *
 * @param[in]  p_client  Identifies client instance for which the procedure is requested.
 *                       Shall not be NULL.
 * @param[in]  p_param   Subscription parameters. Shall not be NULL.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 */
uint32_t mqtt_subscribe(mqtt_client_t                  * const p_client,
                        mqtt_subscription_list_t const * const p_param);


/**
 * @brief API to request un-subscribe from a topic on the connection.
 *
 * @param[in]  p_client  Identifies client instance for which the procedure is requested.
 *                       Shall not be NULL.
 * @param[in]  p_param   Parameters describing topics being unsubscribed from.
 *                       Shall not be NULL.
 *
 * @note QoS included in topic description is unused in this API.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 */
uint32_t mqtt_unsubscribe(mqtt_client_t                  * const p_client,
                          mqtt_subscription_list_t const * const p_param);


/**
 * @brief API to abort MQTT connection.
 *
 * @param[in]  p_client  Identifies client instance for which procedure is requested.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 */
uint32_t mqtt_abort(mqtt_client_t * const p_client);


/**
 * @brief API to disconnect MQTT connection.
 *
 * @param[in]  p_client  Identifies client instance for which procedure is requested.
 *
 * @retval NRF_SUCCESS or an error code indicating reason for failure.
 */
uint32_t mqtt_disconnect(mqtt_client_t * const p_client);


/**
 * @brief This API should be called periodically for the module to be able to keep the connection
 *        alive by sending Ping Requests if need be.
 *
 * @note  Application shall ensure that the periodicity of calling this function makes it possible to
 *        respect the Keep Alive time agreed with the broker on connection.
 *        @ref mqtt_connect for details on Keep Alive time.
 *
 * @retval NRF_SUCCESS or an result code indicating reason for failure.
 */
uint32_t mqtt_live(void);


#endif /* MQTT_H_ */

/**@}  */

