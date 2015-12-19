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

/**@file
 *
 * @defgroup iot_error IoT SDK error codes
 * @{
 * @ingroup iot_sdk_common
 * @{
 * @brief Error codes for the nRF5x IoT SDK.
 *
 * @details 
 * Error codes are 32-bit unsigned integers. The most significant 16 bits (MSB) are reserved for
 *          identifying the module where the error occurred. The least significant 16 bits (LSB)
 *          are used to provide the cause or nature of error. Each module is assigned a 16-bit
 *          unsigned integer, which it will use to identify all errors that occurred in the module.
 *          For example, if 0x8800 identifies a certain SDK module, all error codes from 
 *          0x88000000 to 0x8800FFFF are reserved for this module.
 *
 *          Note that common error reasons have been assigned values to make it 
 *          possible to decode error reasons easily. As an example, if a module is not initialized, 
 *          this error is assigned the error code 0x000A0. If an application encounters an error code
 *          0xZZZZ00A0, the application knows that it tried to access a certain module without 
 *          initializing it.
 *
 *          Each module can define error codes that are not covered by
 *          the common codes. These values must be defined in a range that does not conflict
 *          with the common error values. Such module-specific error values might be used by
 *          different modules to indicate errors of very different nature; so the same error code LSB 
 *          does not necessarily indicate the same error. If an error is already defined by the NRF 
 *          common error codes, however, these codes are reused.
 *
 *          A specific range is also reserved for the application. The application can use this range 
 *          to define application-specific errors.
 *
 *          The success code NRF_SUCCESS does not include a module identifier.          

 */

#ifndef IOT_ERRORS_H__
#define IOT_ERRORS_H__

#include "sdk_errors.h"

/**
 * @defgroup sdk_iot_err_base Base defined for IoT SDK Modules
 * @{
 */
#define IOT_ERR_BASE                       IOT_ERR_BASE_START
/* @} */


/**
 * @defgroup sdk_iot_module_codes Module codes
 * @brief Codes reserved as identification for the module where the error occurred.
 * @{
 */
#define BLE_IPSP_ERR_BASE                  (IOT_ERR_BASE+0x0100)
#define BLE_6LOWPAN_ERR_BASE               (IOT_ERR_BASE+0x0200)

#define IOT_PBUFFER_ERR_BASE               (IOT_ERR_BASE+0x1000)
#define IOT_CONTEXT_MANAGER_ERR_BASE       (IOT_ERR_BASE+0x1100)
#define IOT_TIMER_ERR_BASE                 (IOT_ERR_BASE+0x1200)
#define IOT_FILE_ERR_BASE                  (IOT_ERR_BASE+0x1300)
#define IOT_DFU_ERR_BASE                   (IOT_ERR_BASE+0x1400)
#define IOT_IPV6_ERR_BASE                  (IOT_ERR_BASE+0x2000)
#define IOT_ICMP6_ERR_BASE                 (IOT_ERR_BASE+0x2100)
#define IOT_UDP6_ERR_BASE                  (IOT_ERR_BASE+0x2200)
#define IOT_COAP_ERR_BASE                  (IOT_ERR_BASE+0x2300)
#define IOT_DNS6_ERR_BASE                  (IOT_ERR_BASE+0x2400)
#define IOT_NTP_ERR_BASE                   (IOT_ERR_BASE+0x2500)
#define IOT_LWM2M_ERR_BASE                 (IOT_ERR_BASE+0x2600)
#define IOT_SOCKET_ERR_BASE                (IOT_ERR_BASE+0x2700)
#define IOT_TFTP_ERR_BASE                  (IOT_ERR_BASE+0x2800)
#define IOT_MQTT_ERR_BASE                  (IOT_ERR_BASE+0x2900)
#define IOT_TLS_ERR_BASE                   (IOT_ERR_BASE+0x2A00)
/* @} */


/**
 * @defgroup iot_sdk_common_errors Common error codes
 * @brief Codes reserved as identification for common errors.
 * @{
 */
#define SDK_ERR_MODULE_NOT_INITIALZED      (SDK_COMMON_ERROR_BASE+0x0000)
#define SDK_ERR_MUTEX_INIT_FAILED          (SDK_COMMON_ERROR_BASE+0x0001)
#define SDK_ERR_MUTEX_LOCK_FAILED          (SDK_COMMON_ERROR_BASE+0x0002)
#define SDK_ERR_MUTEX_UNLOCK_FAILED        (SDK_COMMON_ERROR_BASE+0x0003)
#define SDK_ERR_MUTEX_COND_INIT_FAILED     (SDK_COMMON_ERROR_BASE+0x0004)
#define SDK_ERR_MODULE_ALREADY_INITIALZED  (SDK_COMMON_ERROR_BASE+0x0005)
#define SDK_ERR_API_NOT_IMPLEMENTED        (SDK_COMMON_ERROR_BASE+0x0010)
#define SDK_ERR_FEATURE_NOT_ENABLED        (SDK_COMMON_ERROR_BASE+0x0011)
#define SDK_ERR_RX_PKT_TRUNCATED           (SDK_COMMON_ERROR_BASE+0x0012) 
/* @} */


/**
 * @defgroup ble_ipsp_errors IPSP codes
 * @brief Error and status codes specific to IPSP.
 * @{
 */
#define BLE_IPSP_RX_PKT_TRUNCATED          (BLE_IPSP_ERR_BASE+0x0040)
#define BLE_IPSP_CHANNEL_ALREADY_EXISTS    (BLE_IPSP_ERR_BASE+0x0040)
#define BLE_IPSP_LINK_DISCONNECTED         (BLE_IPSP_ERR_BASE+0x0041)
/* @} */


/**
 * @defgroup ble_6lowpan_errors 6LoWPAN codes
 * @brief Error and status codes specific to 6LoWPAN.
 * @{
 */
#define BLE_6LOWPAN_CID_NOT_FOUND          (BLE_6LOWPAN_ERR_BASE+0x0040)
#define BLE_6LOWPAN_TX_FIFO_FULL           (BLE_6LOWPAN_ERR_BASE+0x0041)
/* @} */


/**
 * @defgroup iot_sdk_ipv6 IPv6 codes
 * @brief Error and status codes specific to IPv6.
 * @{
 */
#define IOT_IPV6_ERR_ADDR_IF_MISMATCH      (IOT_IPV6_ERR_BASE+0x0040)
#define IOT_IPV6_ERR_PENDING               (IOT_IPV6_ERR_BASE+0x0041)
/* @} */


/**
 * @defgroup iot_sdk_udp_errors UDP codes
 * @brief Error and status codes specific to UDP.
 * @{
 */
#define UDP_PORT_IN_USE                    (IOT_UDP6_ERR_BASE+0x0040)
#define UDP_BAD_CHECKSUM                   (IOT_UDP6_ERR_BASE+0x0041)
#define UDP_TRUNCATED_PACKET               (IOT_UDP6_ERR_BASE+0x0042)
#define UDP_MALFORMED_PACKET               (IOT_UDP6_ERR_BASE+0x0043)
#define UDP_INTERFACE_NOT_READY            (IOT_UDP6_ERR_BASE+0x0044)
/* @} */

/**
 * @defgroup iot_sdk_socket_errors socket error codes
 * @brief Error and status codes specific to socket API.
 * @{
 */
#define SOCKET_PORT_IN_USE                  (IOT_SOCKET_ERR_BASE + 0x0040)
#define SOCKET_NO_AVAILABLE_PORTS           (IOT_SOCKET_ERR_BASE + 0x0041)
#define SOCKET_WOULD_BLOCK                  (IOT_SOCKET_ERR_BASE + 0x0042)
#define SOCKET_NO_MEM                       (IOT_SOCKET_ERR_BASE + 0x0043)
#define SOCKET_NO_ROUTE                     (IOT_SOCKET_ERR_BASE + 0x0044)
#define SOCKET_TIMEOUT                      (IOT_SOCKET_ERR_BASE + 0x0045)
#define SOCKET_INTERFACE_NOT_READY          (IOT_SOCKET_ERR_BASE + 0x0046)
#define SOCKET_INVALID_PARAM                (IOT_SOCKET_ERR_BASE + 0x0047)
#define SOCKET_UNSUPPORTED_PROTOCOL         (IOT_SOCKET_ERR_BASE + 0x0048)
/* @} */

/**
 * @defgroup iot_sdk_icmp6_errors ICMP codes
 * @brief Error and status codes specific to ICMP.
 * @{
 */
#define ICMP6_UNHANDLED_PACKET_TYPE        (IOT_ICMP6_ERR_BASE+0x0040)
#define ICMP6_BAD_CHECKSUM                 (IOT_ICMP6_ERR_BASE+0x0041)
#define ICMP6_MALFORMED_PACKET             (IOT_ICMP6_ERR_BASE+0x0042)
#define ICMP6_INVALID_PACKET_DATA          (IOT_ICMP6_ERR_BASE+0x0043)
/* @} */


/**
 * @defgroup iot_sdk_coap_errros CoAP codes
 * @brief Error and status codes specific to CoAP.
 * @{
 */
#define COAP_MESSAGE_ERROR_NULL            (IOT_COAP_ERR_BASE+0x0040)
#define COAP_MESSAGE_ERROR_INVALID_CONF    (IOT_COAP_ERR_BASE+0x0041)
#define COAP_MESSAGE_INVALID_CONTENT       (IOT_COAP_ERR_BASE+0x0042)
#define COAP_TRANSMISSION_RESET_BY_PEER    (IOT_COAP_ERR_BASE+0x0043)
#define COAP_TRANSMISSION_TIMEOUT          (IOT_COAP_ERR_BASE+0x0044)
#define COAP_TRANSPORT_SECURITY_MISSING    (IOT_COAP_ERR_BASE+0x0045) 
/* @} */


/**
 * @defgroup iot_sdk_dns6_errors DNS codes.
 * @brief Error and status codes specific to DNS.
 * @{
 */
#define DNS6_SERVER_UNREACHABLE            (IOT_DNS6_ERR_BASE+0x0040)
#define DNS6_FORMAT_ERROR                  (IOT_DNS6_ERR_BASE+0x0041)
#define DNS6_SERVER_FAILURE                (IOT_DNS6_ERR_BASE+0x0042)
#define DNS6_HOSTNAME_NOT_FOUND            (IOT_DNS6_ERR_BASE+0x0043)
#define DNS6_NOT_IMPLEMENTED               (IOT_DNS6_ERR_BASE+0x0044)
#define DNS6_REFUSED_ERROR                 (IOT_DNS6_ERR_BASE+0x0045)
#define DNS6_RESPONSE_TRUNCATED            (IOT_DNS6_ERR_BASE+0x0046)
/* @} */


/**
 * @defgroup iot_sdk_ntp_errors NTP codes.
 * @brief Error and status codes specific to NTP.
 * @{
 */
#define NTP_SERVER_UNREACHABLE            (IOT_NTP_ERR_BASE+0x0040)
#define NTP_SERVER_BAD_RESPONSE           (IOT_NTP_ERR_BASE+0x0041)
#define NTP_SERVER_KOD_PACKET_RECEIVED    (IOT_NTP_ERR_BASE+0x0042)
/* @} */

/**
 * @defgroup iot_sdk_tftp_errors TFTP codes.
 * @brief Error and status codes specific to TFTP.
 * @{
 */
 /**@brief TFTP Error codes. */
#define TFTP_UNDEFINED_ERROR              (IOT_TFTP_ERR_BASE+0x0040)
#define TFTP_FILE_NOT_FOUND               (IOT_TFTP_ERR_BASE+0x0041)
#define TFTP_ACCESS_DENIED                (IOT_TFTP_ERR_BASE+0x0042)
#define TFTP_FULL_STORAGE                 (IOT_TFTP_ERR_BASE+0x0043)
#define TFTP_INVALID_OPCODE               (IOT_TFTP_ERR_BASE+0x0044)
#define TFTP_INVALID_TID                  (IOT_TFTP_ERR_BASE+0x0045)
#define TFTP_FILE_EXSISTS                 (IOT_TFTP_ERR_BASE+0x0046)
#define TFTP_WRONG_USERNAME               (IOT_TFTP_ERR_BASE+0x0047)
#define TFTP_OPTION_REJECT                (IOT_TFTP_ERR_BASE+0x0048)
#define TFTP_INVALID_PACKET               (IOT_TFTP_ERR_BASE+0x0049)
#define TFTP_REMOTE_UNREACHABLE           (IOT_TFTP_ERR_BASE+0x004A)
 /* @} */

/**@defgroup iot_sdk_mqtt_err_code MQTT Error Codes
 *@{
 */
#define MQTT_ERR_NOT_CONNECTED            (IOT_MQTT_ERR_BASE+0x0040)
#define MQTT_ERR_WRITE                    (IOT_MQTT_ERR_BASE+0x0041)
#define MQTT_ERR_TCP_PROC_FAILED          (IOT_MQTT_ERR_BASE+0x0042)
#define MQTT_CONNECTION_FAILED            (IOT_MQTT_ERR_BASE+0x0043)
#define MQTT_ERR_TRANSPORT_CLOSED         (IOT_MQTT_ERR_BASE+0x0044)
/**@}
 */
 
 
/**@defgroup iot_sdk_tls_err_code NRF TLS Interface Error Codes
 *@{
 */
#define NRF_TLS_CONFIGURATION_FAILED      (IOT_TLS_ERR_BASE+0x0040)
#define NRF_TLS_CONTEXT_SETUP_FAILED      (IOT_TLS_ERR_BASE+0x0041)
#define NRF_TLS_HANDSHAKE_IN_PROGRESS     (IOT_TLS_ERR_BASE+0x0042)
#define NRF_TLS_NO_FREE_INSTANCE          (IOT_TLS_ERR_BASE+0x0043)
#define NRF_TLS_INVALID_CA_CERTIFICATE    (IOT_TLS_ERR_BASE+0x0044)
#define NRF_TLS_OWN_CERT_SETUP_FAILED     (IOT_TLS_ERR_BASE+0x0045)
/**@}
 */
 
/** @} */
/** @} */

#endif // IOT_ERRORS_H__
