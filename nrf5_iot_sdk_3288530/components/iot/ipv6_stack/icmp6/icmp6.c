/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "icmp6_api.h"
#include "icmp6.h"
#include "app_trace.h"
#include "iot_context_manager.h"
#include "ipv6_utils.h"
#include "iot_common.h"

/**
 * @defgroup ble_sdk_icmp6 Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests. These are intended for debugging purposes and
 *          can be enabled by defining the ICMP6_ENABLE_LOGS.
 * @note That if ENABLE_DEBUG_LOG_SUPPORT is disabled, having ICMP6_ENABLE_LOGS has no effect.
 * @{
 */

#if (ICMP6_ENABLE_LOGS == 1)

#define ICMP6_TRC  app_trace_log                                                                    /**< Used for getting trace of execution in the module. */
#define ICMP6_DUMP app_trace_dump                                                                   /**< Used for dumping octet information to get details of bond information etc. */
#define ICMP6_ERR  app_trace_log                                                                    /**< Used for logging errors in the module. */

#else // ICMP6_ENABLE_LOGS

#define ICMP6_TRC(...)                                                                              /**< Diasbles traces. */
#define ICMP6_DUMP(...)                                                                             /**< Diasbles dumping of octet streams. */
#define ICMP6_ERR(...)                                                                              /**< Diasbles error logs. */

#endif // ICMP6_ENABLE_LOGS
/** @} */

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          ICMP6_DISABLE_API_PARAM_CHECK should be set to 1 to disable these checks.
 *
 * @{
 */
#if (ICMP6_DISABLE_API_PARAM_CHECK == 0)

/**@brief Macro to check is module is initialized before requesting one of the module procedures. */
#define VERIFY_MODULE_IS_INITIALIZED()                                                             \
        if(m_initialization_state == false)                                                        \
        {                                                                                          \
            return (SDK_ERR_MODULE_NOT_INITIALZED | IOT_ICMP6_ERR_BASE);                           \
        }

/**@brief Macro to check is module is initialized before requesting one of the module
         procedures but does not use any return code. */
#define VERIFY_MODULE_IS_INITIALIZED_VOID()                                                        \
        if(m_initialization_state == false)                                                        \
        {                                                                                          \
            return;                                                                                \
        }

/**
 * @brief Verify NULL parameters are not passed to API by application.
 */
#define NULL_PARAM_CHECK(PARAM)                                                                    \
        if ((PARAM) == NULL)                                                                       \
        {                                                                                          \
            return (NRF_ERROR_NULL | IOT_ICMP6_ERR_BASE);                                          \
        }
 
/**
 * @brief Verify packet buffer is of ICMP6 Type.
 */
#define PACKET_TYPE_CHECK(PACKET)                                                                  \
        if ((PACKET)->type != ICMP6_PACKET_TYPE)                                                   \
        {                                                                                          \
            return (NRF_ERROR_INVALID_PARAM | IOT_ICMP6_ERR_BASE);                                 \
        } 

        
#else // ICMP6_DISABLE_API_PARAM_CHECK

#define VERIFY_MODULE_IS_INITIALIZED()
#define VERIFY_MODULE_IS_INITIALIZED_VOID()
#define NULL_PARAM_CHECK(PARAM) 
#define PACKET_TYPE_CHECK(PACKET)

#endif // ICMP6_DISABLE_API_PARAM_CHECK
/** @} */

/**
 * @defgroup ble_ipsp_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case need arises to use an alternative architecture.
 * @{
 */
#define ICMP6_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_icmp6_mutex)                                          /**< Lock module using mutex */
#define ICMP6_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_icmp6_mutex)                                        /**< Unlock module using mutex */
/** @} */

#define ND_NS_HEADER_SIZE              20                                                           /**< Size of Neighbour Solicitation message. */
#define ND_NA_HEADER_SIZE              20                                                           /**< Size of Neighbour Advertisement message. */
#define ND_RS_HEADER_SIZE              4                                                            /**< Size of Router Solicitation message. */
#define ND_RA_HEADER_SIZE              12                                                           /**< Size of Router Advertisement message. */

#define ND_OPT_TYPE_SLLAO              1                                                            /**< Source Link Layer Address Option. */
#define ND_OPT_TYPE_TLLAO              2                                                            /**< Target Link Layer Address Option. */
#define ND_OPT_TYPE_PIO                3                                                            /**< Prefix Information Option. */
#define ND_OPT_TYPE_RHO                4                                                            /**< Redirected Header Option. */
#define ND_OPT_TYPE_MTU                5                                                            /**< Maximum Transmit Unit Option. */
#define ND_OPT_TYPE_ARO                33                                                           /**< Address Registration Option. */
#define ND_OPT_TYPE_6CO                34                                                           /**< 6LoWPAN Context Option. */
#define ND_OPT_TYPE_6ABRO              35                                                           /**< Authoritative Border Router Option. */

#define ND_OPT_SLLAO_LENGTH            2                                                            /**< Value of length field in SLLAO option. */
#define ND_OPT_ARO_LENGTH              2                                                            /**< Value of length field in ARO option. */

#define ND_OPT_SLLAO_SIZE              16                                                           /**< Size of SLLAO option. */
#define ND_OPT_PIO_SIZE                32                                                           /**< Size of PIO option. */
#define ND_OPT_MTU_SIZE                8                                                            /**< Size of MTU option. */
#define ND_OPT_ARO_SIZE                16                                                           /**< Size of ARO option. */
#define ND_OPT_6CO_SIZE                24                                                           /**< Size of 6CO option. */
#define ND_OPT_6ABRO_SIZE              24                                                           /**< Size of 6ABRO option. */

#define ND_OPT_6CO_CID_MASK            0x0F
#define ND_OPT_6CO_CID_POS             0
#define ND_OPT_6CO_C_MASK              0x10
#define ND_OPT_6CO_C_POS               4

#define ND_OPT_PIO_L_MASK               0x80
#define ND_OPT_PIO_L_POS               7
#define ND_OPT_PIO_A_MASK               0x40
#define ND_OPT_PIO_A_POS               6

#define ND_HOP_LIMIT                   255                                                          /**< Value of Hop Limit used in Neighbour Discovery procedure. */

#define ICMP6_OFFSET                   IPV6_IP_HEADER_SIZE + ICMP6_HEADER_SIZE                      /**< Offset of ICMPv6 packet type. */

/**@brief Echo Header structure used in echo request and echo reply messages. */
typedef struct
{
    uint16_t identifier;                                                                            /**< Echo Identifier. */
    uint16_t sequence_number;                                                                       /**< Sequence Number of Echo message. */
} icmp6_echo_header_t;

/**@brief Neighbour Solicitation message's header. */
typedef struct
{
    uint32_t    reserved;                                                                           /**< Reserved field. */
    ipv6_addr_t target_addr;                                                                        /**< Target Address field. */
} icmp6_ns_header_t;

/**@brief Router Solicitation message's header. */
typedef struct
{
    uint32_t reserved;                                                                              /**< Reserved field. */
} icmp6_rs_header_t;

/**@brief Option header of ICMPv6 packet. */
typedef struct
{
    uint8_t type;                                                                                   /**< Option type. */
    uint8_t length;                                                                                 /**< Length, in unit of 8 octets. */
} nd_option_t;

/**@brief Source Link Layer Address Option header format. */
typedef struct
{
    uint8_t type;                                                                                   /**< Option type. */
    uint8_t length;                                                                                 /**< Length, units of 8 octets. */
    eui64_t addr;                                                                                   /**< Link-layer address. */
    uint8_t padding[6];                                                                             /**< Padding. */
} nd_option_sllao_t;

/**@brief Prefix Information Option header format. */
typedef struct
{
    uint8_t     type;                                                                               /**< Option type. */
    uint8_t     length;                                                                             /**< Length, units of 8 octets. */
    uint8_t     prefix_length;                                                                      /**< Prefix length. */
    uint8_t     flags;                                                                              /**< Flags (L/A) and reserved. */
    uint32_t    valid_lifetime;                                                                     /**< Valid Lifetime.  */
    uint32_t    preferred_lifetime;                                                                 /**< Preferred Lifetime. */
    uint32_t    reserved;                                                                           /**< Reserved field. */
    ipv6_addr_t prefix;                                                                             /**< Prefix address. */
} nd_option_pio_t;

/**@brief Address Registration Option header format. */
typedef struct
{
    uint8_t  type;                                                                                  /**< Option type. */
    uint8_t  length;                                                                                /**< Length, units of 8 octets. */
    uint8_t  status;                                                                                /**< Status of ARO. */
    uint8_t  reserved;                                                                              /**< Reserved1, split to avoid alignment. */
    uint16_t reserved2;                                                                             /**< Reserved2, split to avoid alignment. */
    uint16_t registration_lifetime;                                                                 /**< Registration Lifetime. */
    eui64_t  eui64;                                                                                 /**< EUI-64 source address. */
} nd_option_aro_t;

/**@brief 6LoWPAN Context Option header format. */
typedef struct
{
    uint8_t     type;                                                                               /**< Option type. */
    uint8_t     length;                                                                             /**< Length, units of 8 octets. */
    uint8_t     context_length;                                                                     /**< Context Length. */
    uint8_t     CID_C;                                                                              /**< 4-bit Context and 1-bit context compression flag. */
    uint16_t    reserved;                                                                           /**< Reserved. */
    uint16_t    valid_lifetime;                                                                     /**< Valid Lifetime. */
    ipv6_addr_t context;                                                                            /**< Context IPv6 Prefix. */
} nd_option_6co_t;


static bool                     m_initialization_state = false;                                     /**< Variable to maintain module initialization state. */
static uint16_t                 m_sequence_number      = 0;                                         /**< Sequence number from ICMPv6 packet. */
static icmp6_receive_callback_t m_event_handler        = NULL;                                      /**< Application event handler. */
SDK_MUTEX_DEFINE(m_icmp6_mutex)                                                                     /**< Mutex variable. Currently unused, this declaration does not occupy any space in RAM. */

/**@brief Function for initializing default values of IP Header for ICMP.
 *
 * @param[in]   p_ip_header   Pointer to IPv6 header.
 *
 * @return      None. 
 */
static __INLINE void icmp_ip_header(ipv6_header_t * p_ip_header, uint8_t hoplimit)
{
    ipv6_header_init(p_ip_header);
    p_ip_header->next_header = IPV6_NEXT_HEADER_ICMP6;
    p_ip_header->hoplimit    = hoplimit;  
}

/**@brief Function for adding SLLAO option to packet.
 *
 * @param[in]   p_interface  Pointer to IoT interface.
 * @param[in]   p_data       Pointer to memory to place SLLAO option.
 *
 * @return      None.
 */
static __INLINE void add_sllao_opt(const iot_interface_t * p_interface, nd_option_sllao_t * p_sllao)
{    
    p_sllao->type   = ND_OPT_TYPE_SLLAO;
    p_sllao->length = ND_OPT_SLLAO_LENGTH;

    // Copy EUI-64 and add padding.
    memcpy(p_sllao->addr.identifier, p_interface->local_addr.identifier, EUI_64_ADDR_SIZE);
    memset(p_sllao->padding, 0, 6);
}

/**@brief Function for adding ARO option to packet.
 *
 * @param[in]   p_interface  Pointer to IoT interface.
 * @param[in]   p_data       Pointer to memory to place SLLAO option.
 * @param[in]   aro_lifetime Lifetime of registration.
 *
 * @return      None. 
 */
static __INLINE void add_aro_opt(const iot_interface_t * p_interface,
                                 nd_option_aro_t * p_aro, 
                                 uint16_t          aro_lifetime)
{
    p_aro->type                  = ND_OPT_TYPE_ARO;
    p_aro->length                = ND_OPT_ARO_LENGTH;
    p_aro->status                = 0x00;
    p_aro->reserved              = 0x00;
    p_aro->reserved2             = 0x00;
    p_aro->registration_lifetime = HTONS(aro_lifetime);

    // Copy EUI-64 and add padding.
    memcpy(p_aro->eui64.identifier, p_interface->local_addr.identifier, EUI_64_ADDR_SIZE);
}

#if (ICMP6_ENABLE_ND6_MESSAGES_TO_APPLICATION == 1 || ICMP6_ENABLE_ALL_MESSAGES_TO_APPLICATION == 1)

/**@brief Function for notifying application of the ICMPv6 received packet.
 *
 * @param[in]   p_interface    Pointer to external interface from which packet come.
 * @param[in]   p_pbuffer      Pointer to packet buffer of ICMP6_PACKET_TYPE.
 * @param[in]   process_result Result of internal processing packet.
 *
 * @return      NRF_SUCCESS after successful processing, error otherwise. 
 */
static uint32_t app_notify_icmp_data(iot_interface_t  * p_interface, 
                                 iot_pbuffer_t    * p_pbuffer,
                                 uint32_t           process_result)
{
    uint32_t err_code = NRF_SUCCESS;

    if(m_event_handler != NULL)
    {

       ipv6_header_t  * p_ip_header   = (ipv6_header_t *)
                               (p_pbuffer->p_payload - ICMP6_HEADER_SIZE - IPV6_IP_HEADER_SIZE);
       icmp6_header_t * p_icmp_header = (icmp6_header_t *)
                               (p_pbuffer->p_payload - ICMP6_HEADER_SIZE);

        ICMP6_MUTEX_UNLOCK();
        
        // Change byte order of ICMP header given to application.
        p_icmp_header->checksum = NTOHS(p_icmp_header->checksum);

        err_code = m_event_handler(p_interface,
                                   p_ip_header,
                                   p_icmp_header,
                                   process_result,
                                   p_pbuffer);

        ICMP6_MUTEX_LOCK();
    }

    return err_code;
}

#endif

#if (ICMP6_ENABLE_HANDLE_ECHO_REQUEST_TO_APPLICATION == 0)

/**@brief Function for responding on ECHO REQUEST message.
 *
 * @param[in]   p_interface   Pointer to external interface from which packet come.
 * @param[in]   p_ip_header   Pointer to IPv6 Header.
 * @param[in]   p_icmp_header Pointer to ICMPv6 header.
 * @param[in]   p_packet      Pointer to packet buffer.
 *
 * @return      NRF_SUCCESS after successful processing, error otherwise. 
 */
static void echo_reply_send(iot_interface_t  * p_interface,
                            ipv6_header_t    * p_ip_header,
                            icmp6_header_t   * p_icmp_header,
                            iot_pbuffer_t    * p_packet)
{
    uint32_t                    err_code;
    uint16_t                    checksum;
    iot_pbuffer_t             * p_pbuffer;
    iot_pbuffer_alloc_param_t   pbuff_param;

    // Headers of new packet.
    ipv6_header_t       * p_reply_ip_header;
    icmp6_header_t      * p_reply_icmp_header;
    icmp6_echo_header_t * p_reply_echo_header;
    icmp6_echo_header_t * p_echo_header = (icmp6_echo_header_t *)(p_packet->p_payload);

    ICMP6_TRC("[ICMP6]: Sending reply on Echo Request.\r\n");

    // Requesting buffer for reply
    pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
    pbuff_param.type   = ICMP6_PACKET_TYPE;
    pbuff_param.length = p_packet->length;  
        
    // err_code = iot_pbuffer_reallocate(&pbuff_param, p_pbuffer);
    // This will be available after deciding when freeing received
    // packet. Because when we do reallocate, which cause using same buffer,
    // We can free it before sending procedure occurs!
    // Now we free the memory at the end of ipv6_input.
    err_code = iot_pbuffer_allocate(&pbuff_param, &p_pbuffer);

    if(err_code == NRF_SUCCESS)
    {
        p_reply_ip_header   = (ipv6_header_t *)(p_pbuffer->p_payload - ICMP6_HEADER_SIZE -
                                               IPV6_IP_HEADER_SIZE);
        p_reply_icmp_header = (icmp6_header_t *)(p_pbuffer->p_payload - ICMP6_HEADER_SIZE);
        p_reply_echo_header = (icmp6_echo_header_t *)(p_pbuffer->p_payload);

        // Change ICMP header.
        p_reply_icmp_header->type     = ICMP6_TYPE_ECHO_REPLY;
        p_reply_icmp_header->code     = 0;
        p_reply_icmp_header->checksum = 0;

        // IPv6 Header initialization.
        icmp_ip_header(p_reply_ip_header, IPV6_DEFAULT_HOP_LIMIT);

        p_reply_ip_header->destaddr = p_ip_header->srcaddr;
        p_reply_ip_header->length   = HTONS(p_pbuffer->length + ICMP6_HEADER_SIZE);

        if(IPV6_ADDRESS_IS_MULTICAST(&p_ip_header->destaddr))
        {
            IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&p_reply_ip_header->srcaddr, 
                                              p_interface->local_addr.identifier);
        }
        else
        {
            p_reply_ip_header->srcaddr = p_ip_header->destaddr;
        }

        // Set echo reply parameters.
        p_reply_echo_header->identifier      = p_echo_header->identifier;
        p_reply_echo_header->sequence_number = p_echo_header->sequence_number;

        // Copy user data.       
        memcpy(p_pbuffer->p_payload + ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET,
               p_packet->p_payload  + ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET,
               p_packet->length     - ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET);

        // Calculate checksum.
        checksum = p_pbuffer->length + ICMP6_HEADER_SIZE + IPV6_NEXT_HEADER_ICMP6;

        ipv6_checksum_calculate(p_reply_ip_header->srcaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
        ipv6_checksum_calculate(p_reply_ip_header->destaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
        ipv6_checksum_calculate(p_pbuffer->p_payload - ICMP6_HEADER_SIZE,
                                p_pbuffer->length + ICMP6_HEADER_SIZE, 
                                &checksum,
                                false);

        p_reply_icmp_header->checksum = HTONS((~checksum));

        p_pbuffer->p_payload -= ICMP6_OFFSET;
        p_pbuffer->length    += ICMP6_OFFSET;

        // Send IPv6 packet.
        err_code = ipv6_send(p_interface, p_pbuffer);

        if(err_code != NRF_SUCCESS)
        {
            ICMP6_ERR("[ICMP6]: Cannot send packet buffer!\r\n");
        }
    }
    else
    {
        ICMP6_ERR("[ICMP6]: Failed to allocate packet buffer!\r\n");   
    }
}
#endif


/**@brief Function for parsing Router Advertisement message.
 *        Because stack gives all control to application, internal RA parsing take care
 *        only on Context Identifier.
 *
 * @param[in]   p_interface   Pointer to external interface from which packet come.
 * @param[in]   p_ip_header   Pointer to IPv6 Header.
 * @param[in]   p_icmp_header Pointer to ICMPv6 header.
 * @param[in]   p_packet      Pointer to packet buffer.
 *
 * @return      NRF_SUCCESS after successful processing, error otherwise. 
 */
static uint32_t ra_input(iot_interface_t  * p_interface,
                         ipv6_header_t    * p_ip_header,
                         icmp6_header_t   * p_icmp_header,
                         iot_pbuffer_t    * p_packet)
{
    uint32_t          err_code;
    iot_context_t     context;
    iot_context_t   * p_context;
    uint16_t          curr_opt_offset = ND_RA_HEADER_SIZE;
    nd_option_t     * p_opt           = NULL;
    nd_option_6co_t * p_6co           = NULL;
    nd_option_pio_t * p_pio           = NULL;

    if(!IPV6_ADDRESS_IS_LINK_LOCAL(&p_ip_header->srcaddr))
    {
        return ICMP6_INVALID_PACKET_DATA;
    }

    // Read all option we get.
    while(curr_opt_offset < p_packet->length)
    {
        p_opt = (nd_option_t *)(p_packet->p_payload + curr_opt_offset);

        if(p_opt->length == 0)
        {
            ICMP6_ERR("[ICMP6]: Option can't has 0 bytes!\r\n"); 
            return ICMP6_INVALID_PACKET_DATA;
        }

        ICMP6_ERR("[ICMP6]: Option type = 0x%02x!\r\n", p_opt->type);

        // Searching for handling options. 
        switch(p_opt->type)
        {
            case ND_OPT_TYPE_PIO:
            {
                p_pio = (nd_option_pio_t *)p_opt;

                if(p_pio->prefix_length != 0 && 
                  (p_pio->flags & ND_OPT_PIO_A_MASK) &&
                  !(p_pio->flags & ND_OPT_PIO_L_MASK))
                {
                    // Ignore Link-Local address
                    if(IPV6_ADDRESS_IS_LINK_LOCAL(&p_pio->prefix))
                    {
                        ICMP6_ERR("[ICMP6]: Ignore Link-Local prefix!\r\n");
                        break;
                    }

                    // For now address is automatically set as a preferred.
                    ipv6_addr_conf_t temp_address;

                    // Set IPv6 EUI-64
                    IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&temp_address.addr, p_interface->local_addr.identifier);
                    
                    // Add prefix
                    IPV6_ADDRESS_PREFIX_SET(temp_address.addr.u8, p_pio->prefix.u8, p_pio->prefix_length);

                    if(p_pio->valid_lifetime != 0)
                    {
                        temp_address.state = IPV6_ADDR_STATE_PREFERRED;

                        err_code = ipv6_address_set(p_interface, &temp_address);

                        if(err_code != NRF_SUCCESS)
                        {
                            ICMP6_ERR("[ICMP6]: Cannot add new address! Address table is full!\r\n"); 
                        }
                    }
                    else
                    {
                        err_code = ipv6_address_remove(p_interface, &temp_address.addr);
                        
                        if(err_code != NRF_SUCCESS)
                        {
                            ICMP6_ERR("[ICMP6]: Cannot remove address!\r\n"); 
                        }
                    }
                }
                else
                {
                    ICMP6_ERR("[ICMP6]: Prefix option has incorrect parameters!\r\n"); 
                    return ICMP6_INVALID_PACKET_DATA;
                }

                break;
            }
            case ND_OPT_TYPE_6CO:
            {
                p_6co = (nd_option_6co_t *)p_opt;

                memset(context.prefix.u8, 0, IPV6_ADDR_SIZE);

                context.prefix           = p_6co->context;
                context.prefix_len       = p_6co->context_length;
                context.context_id       = (p_6co->CID_C & ND_OPT_6CO_CID_MASK) >>
                                            ND_OPT_6CO_CID_POS;
                context.compression_flag = (p_6co->CID_C & ND_OPT_6CO_C_MASK) >>
                                            ND_OPT_6CO_C_POS;

                if(p_6co->valid_lifetime == 0)
                {
                    err_code = iot_context_manager_get_by_cid(p_interface,
                                                              context.context_id,
                                                              &p_context);

                    if(err_code == NRF_SUCCESS)
                    {
                        err_code = iot_context_manager_remove(p_interface, p_context);

                        if(err_code == NRF_SUCCESS)
                        {
                            ICMP6_TRC("[ICMP6]: Removed context! CID = 0x%02x\r\n", context.context_id);
                        }
                    }

                }
                else
                {
                    err_code = iot_context_manager_update(p_interface, &context);
                        
                    if(err_code == NRF_SUCCESS)
                    {
                        ICMP6_TRC("[ICMP6]: New context added! CID = 0x%02x\r\n", context.context_id);
                    }
                }

                break;
            }
        }

        // Increment current offset option.
        curr_opt_offset += 8 * p_opt->length;
    }

    return NRF_SUCCESS;
}

/**@brief Function for notifying application of the ICMPv6 received packet.
 *
 * @param[in]   p_interface   Pointer to external interface from which packet come.
 * @param[in]   p_ip_header   Pointer to IPv6 Header.
 * @param[in]   p_icmp_header Pointer to ICMPv6 header.
 * @param[in]   p_packet      Pointer to packet buffer.
 *
 * @return      NRF_SUCCESS after successful processing, error otherwise. 
 */
static uint32_t ndisc_input(iot_interface_t  * p_interface,
                            ipv6_header_t    * p_ip_header,
                            icmp6_header_t   * p_icmp_header,
                            iot_pbuffer_t    * p_packet)
{
    uint32_t process_result;

    switch(p_icmp_header->type)
    {
        case ICMP6_TYPE_ROUTER_SOLICITATION:
            ICMP6_TRC("[ICMP6]: Got unsupported Router Solicitation message.\r\n");

            process_result = ICMP6_UNHANDLED_PACKET_TYPE;
        break;

        case ICMP6_TYPE_ROUTER_ADVERTISEMENT:
            ICMP6_TRC("[ICMP6]: Got Router Advertisement message.\r\n");

            process_result = ra_input(p_interface, p_ip_header, p_icmp_header, p_packet);
        break;

        case ICMP6_TYPE_NEIGHBOR_SOLICITATION:
            ICMP6_TRC("[ICMP6]: Got unsupported Neighbour Solicitation message.\r\n");

            process_result = ICMP6_UNHANDLED_PACKET_TYPE;
        break;

        case ICMP6_TYPE_NEIGHBOR_ADVERTISEMENT:
            ICMP6_TRC("[ICMP6]: Got Neighbour Advertisement message.\r\n");

            process_result = NRF_SUCCESS;
        break;

        default:
            process_result = ICMP6_UNHANDLED_PACKET_TYPE;
            break;
    }

    return process_result;
}


uint32_t icmp6_echo_request(const iot_interface_t  * p_interface,
                            const ipv6_addr_t      * p_src_addr,
                            const ipv6_addr_t      * p_dest_addr,
                            iot_pbuffer_t          * p_request)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_interface);
    NULL_PARAM_CHECK(p_src_addr);
    NULL_PARAM_CHECK(p_dest_addr);
    NULL_PARAM_CHECK(p_request);
    PACKET_TYPE_CHECK(p_request);

    uint32_t              err_code = NRF_SUCCESS;
    uint16_t              checksum;
    ipv6_header_t       * p_ip_header;
    icmp6_header_t      * p_icmp_header;
    icmp6_echo_header_t * p_echo_header;

    ICMP6_MUTEX_LOCK();

    ICMP6_TRC("[ICMP6]: >> icmp6_echo_request\r\n");

    // Headers of IPv6 packet.
    p_ip_header   = (ipv6_header_t *)(p_request->p_payload - ICMP6_HEADER_SIZE -
                                      IPV6_IP_HEADER_SIZE);
    p_icmp_header = (icmp6_header_t *)(p_request->p_payload - ICMP6_HEADER_SIZE);
    p_echo_header = (icmp6_echo_header_t *)(p_request->p_payload);

    // Change ICMP header.
    p_icmp_header->type     = ICMP6_TYPE_ECHO_REQUEST;
    p_icmp_header->code     = 0;
    p_icmp_header->checksum = 0;

    // IPv6 Header initialization.
    icmp_ip_header(p_ip_header, IPV6_DEFAULT_HOP_LIMIT);

    p_ip_header->srcaddr  = *p_src_addr;
    p_ip_header->destaddr = *p_dest_addr;
    p_ip_header->length   = HTONS(p_request->length + ICMP6_HEADER_SIZE);

    // Set echo reply parameters.
    p_echo_header->identifier      = 0;
    p_echo_header->sequence_number = HTONS(m_sequence_number);

    // Calculate checksum.
    checksum = p_request->length + ICMP6_HEADER_SIZE + IPV6_NEXT_HEADER_ICMP6;

    ipv6_checksum_calculate(p_ip_header->srcaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
    ipv6_checksum_calculate(p_ip_header->destaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
    ipv6_checksum_calculate(p_request->p_payload - ICMP6_HEADER_SIZE,
                            p_request->length + ICMP6_HEADER_SIZE,
                            &checksum,
                            false);

    p_icmp_header->checksum = HTONS((~checksum));

    m_sequence_number++;
    p_request->p_payload -= ICMP6_OFFSET;
    p_request->length    += ICMP6_OFFSET;

    // Send IPv6 packet.
    err_code = ipv6_send(p_interface, p_request);

    ICMP6_TRC("[ICMP6]: << icmp6_echo_request\r\n");

    ICMP6_MUTEX_UNLOCK();

    return err_code;
}

uint32_t icmp6_rs_send(const iot_interface_t * p_interface,
                       const ipv6_addr_t      * p_src_addr,
                       const ipv6_addr_t      * p_dest_addr)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_interface);
    NULL_PARAM_CHECK(p_src_addr);
    NULL_PARAM_CHECK(p_dest_addr);

    uint32_t                    err_code = NRF_SUCCESS;
    uint16_t                    checksum;
    iot_pbuffer_t             * p_pbuffer;
    iot_pbuffer_alloc_param_t   pbuff_param;

    // IPv6 Headers.
    ipv6_header_t             * p_ip_header;
    icmp6_header_t            * p_icmp_header;
    icmp6_rs_header_t         * p_rs_header;
    nd_option_sllao_t         * p_sllao_opt;

    ICMP6_MUTEX_LOCK();

    ICMP6_TRC("[ICMP6]: >> icmp6_ndisc_rs_send\r\n");

    // Requesting buffer for RS message
    pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
    pbuff_param.type   = ICMP6_PACKET_TYPE;
    pbuff_param.length = ND_RS_HEADER_SIZE + ND_OPT_SLLAO_SIZE;

    err_code = iot_pbuffer_allocate(&pbuff_param, &p_pbuffer);

    if(err_code == NRF_SUCCESS)
    {
        p_ip_header   = (ipv6_header_t *)(p_pbuffer->p_payload - ICMP6_HEADER_SIZE -
                                          IPV6_IP_HEADER_SIZE);
        p_icmp_header = (icmp6_header_t *)(p_pbuffer->p_payload - ICMP6_HEADER_SIZE);
        p_rs_header   = (icmp6_rs_header_t *)(p_pbuffer->p_payload);
        p_sllao_opt   = (nd_option_sllao_t *)(p_pbuffer->p_payload + ND_RS_HEADER_SIZE);

        // Change ICMP header.
        p_icmp_header->type     = ICMP6_TYPE_ROUTER_SOLICITATION;
        p_icmp_header->code     = 0;
        p_icmp_header->checksum = 0;

        // IPv6 Header initialization.
        icmp_ip_header(p_ip_header, ND_HOP_LIMIT);

        p_ip_header->srcaddr     = *p_src_addr;
        p_ip_header->destaddr    = *p_dest_addr;
        p_ip_header->length      = HTONS(p_pbuffer->length + ICMP6_HEADER_SIZE);

        // Set Router Solicitation parameter.
        p_rs_header->reserved = 0;

        // Add SLLAO option.
        add_sllao_opt(p_interface, p_sllao_opt);

        // Calculate checksum.
        checksum = p_pbuffer->length + ICMP6_HEADER_SIZE + IPV6_NEXT_HEADER_ICMP6;

        ipv6_checksum_calculate(p_ip_header->srcaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
        ipv6_checksum_calculate(p_ip_header->destaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
        ipv6_checksum_calculate(p_pbuffer->p_payload - ICMP6_HEADER_SIZE,
                                p_pbuffer->length + ICMP6_HEADER_SIZE,
                                &checksum,
                                false);

        p_icmp_header->checksum = HTONS((~checksum));

        p_pbuffer->p_payload -= ICMP6_OFFSET;
        p_pbuffer->length    += ICMP6_OFFSET;

        // Send IPv6 packet.
        err_code = ipv6_send(p_interface, p_pbuffer);
    }
    else
    {
        ICMP6_ERR("[ICMP6]: Failed to allocate packet buffer!\r\n");
    }

    ICMP6_TRC("[ICMP6]: << icmp6_ndisc_rs_send\r\n");
    
    ICMP6_MUTEX_UNLOCK();

    return err_code;
}

uint32_t icmp6_ns_send(const iot_interface_t  * p_interface,
                       const ipv6_addr_t      * p_src_addr,
                       const ipv6_addr_t      * p_dest_addr,
                       const icmp6_ns_param_t * p_param)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(p_interface);
    NULL_PARAM_CHECK(p_src_addr);
    NULL_PARAM_CHECK(p_dest_addr);
    NULL_PARAM_CHECK(p_param);

    uint32_t                    err_code = NRF_SUCCESS;
    uint16_t                    aro_size = 0;
    uint16_t                    checksum;
    iot_pbuffer_t             * p_pbuffer;
    iot_pbuffer_alloc_param_t   pbuff_param;

    // IPv6 Headers.
    ipv6_header_t             * p_ip_header;
    icmp6_header_t            * p_icmp_header;
    icmp6_ns_header_t         * p_ns_header;
    nd_option_sllao_t         * p_sllao_opt;
    nd_option_aro_t           * p_aro_opt;

    ICMP6_MUTEX_LOCK();

    ICMP6_TRC("[ICMP6]: >> icmp6_ndisc_ns_send\r\n");

    if(p_param->add_aro)
    {
        aro_size = ND_OPT_ARO_SIZE;
    }

    // Requesting buffer for NS message
    pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
    pbuff_param.type   = ICMP6_PACKET_TYPE;
    pbuff_param.length = ND_NS_HEADER_SIZE + ND_OPT_SLLAO_SIZE + aro_size;

    err_code = iot_pbuffer_allocate(&pbuff_param, &p_pbuffer);

    if(err_code == NRF_SUCCESS)
    {
        p_ip_header   = (ipv6_header_t *)(p_pbuffer->p_payload - ICMP6_HEADER_SIZE -
                                          IPV6_IP_HEADER_SIZE);
        p_icmp_header = (icmp6_header_t *)(p_pbuffer->p_payload - ICMP6_HEADER_SIZE);
        p_ns_header   = (icmp6_ns_header_t *)(p_pbuffer->p_payload);
        p_sllao_opt   = (nd_option_sllao_t *)(p_pbuffer->p_payload + ND_NS_HEADER_SIZE);
        p_aro_opt     = (nd_option_aro_t *)(p_pbuffer->p_payload + ND_NS_HEADER_SIZE +
                                            ND_OPT_SLLAO_SIZE);

        // Change ICMP header.
        p_icmp_header->type     = ICMP6_TYPE_NEIGHBOR_SOLICITATION;
        p_icmp_header->code     = 0;
        p_icmp_header->checksum = 0;

        // IPv6 Header initialization.
        icmp_ip_header(p_ip_header, ND_HOP_LIMIT);

        p_ip_header->srcaddr  = *p_src_addr;
        p_ip_header->destaddr = *p_dest_addr;
        p_ip_header->length   = HTONS(p_pbuffer->length + ICMP6_HEADER_SIZE);

        // Set Neighbour Solicitation parameter.
        p_ns_header->reserved    = 0;
        p_ns_header->target_addr = p_param->target_addr;

        // Add SLLAO option.
        add_sllao_opt(p_interface, p_sllao_opt);

        if(p_param->add_aro)
        {
            add_aro_opt(p_interface, p_aro_opt, p_param->aro_lifetime);
        }

        // Calculate checksum.
        checksum = p_pbuffer->length + ICMP6_HEADER_SIZE + IPV6_NEXT_HEADER_ICMP6;

        ipv6_checksum_calculate(p_ip_header->srcaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
        ipv6_checksum_calculate(p_ip_header->destaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
        ipv6_checksum_calculate(p_pbuffer->p_payload - ICMP6_HEADER_SIZE,
                                p_pbuffer->length + ICMP6_HEADER_SIZE,
                                &checksum,
                                false);

        p_icmp_header->checksum = HTONS((~checksum));

        p_pbuffer->p_payload -= ICMP6_OFFSET;
        p_pbuffer->length    += ICMP6_OFFSET;

        // Send IPv6 packet.
        err_code = ipv6_send(p_interface, p_pbuffer);
    }
    else
    {
        ICMP6_ERR("[ICMP6]: Failed to allocate packet buffer!\r\n");
    }

    ICMP6_TRC("[ICMP6]: << icmp6_ndisc_ns_send\r\n");

    ICMP6_MUTEX_UNLOCK();

    return err_code;
}
                                

uint32_t icmp6_receive_register(icmp6_receive_callback_t cb)
{
    VERIFY_MODULE_IS_INITIALIZED();
    NULL_PARAM_CHECK(cb);
    UNUSED_VARIABLE(m_event_handler);

    ICMP6_MUTEX_LOCK();

    ICMP6_TRC("[ICMP6]: >> icmp6_receive_register\r\n");

    // Store application event handler.
    m_event_handler = cb;

    ICMP6_TRC("[ICMP6]: << icmp6_receive_register\r\n");

    ICMP6_MUTEX_UNLOCK();

    return NRF_SUCCESS;
}


uint32_t icmp6_init(void)
{
    SDK_MUTEX_INIT(m_ipv6_mutex);
    SDK_MUTEX_LOCK();

    ICMP6_TRC("[ICMP6]: >> icmp6_init\r\n");

    // Set application event handler.
    m_event_handler = NULL;

    // Indicate initialization of module.
    m_initialization_state = true;

    ICMP6_TRC("[ICMP6]: << icmp6_init\r\n");

    SDK_MUTEX_UNLOCK();

    return NRF_SUCCESS;
}


uint32_t icmp6_input(iot_interface_t  * p_interface,
                     ipv6_header_t    * p_ip_header,
                     iot_pbuffer_t    * p_packet)
{
    VERIFY_MODULE_IS_INITIALIZED();

    NULL_PARAM_CHECK(p_interface);
    NULL_PARAM_CHECK(p_ip_header);
    NULL_PARAM_CHECK(p_packet);

    uint16_t              checksum;
    uint32_t              process_result = NRF_SUCCESS;
    bool                  is_ndisc       = false;
    icmp6_header_t      * p_icmp_header  = (icmp6_header_t *)p_packet->p_payload;
    icmp6_echo_header_t * p_echo_header  = NULL;
    uint32_t              err_code       = NRF_SUCCESS;

    SDK_MUTEX_LOCK();
      
    ICMP6_TRC("[ICMP6]: >> icmp6_input\r\n");
    
    if(p_packet->length < ICMP6_HEADER_SIZE || p_ip_header->length < ICMP6_HEADER_SIZE)
    {
        ICMP6_ERR("[ICMP6]: Received malformed packet, which has 0x%08lX bytes.\r\n",
                  p_packet->length);
        process_result = ICMP6_MALFORMED_PACKET;
    }
    else
    {
        // Check checksum of packet.
        checksum = p_packet->length + IPV6_NEXT_HEADER_ICMP6;

        ipv6_checksum_calculate(p_ip_header->srcaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
        ipv6_checksum_calculate(p_ip_header->destaddr.u8, IPV6_ADDR_SIZE, &checksum, false);
        ipv6_checksum_calculate(p_packet->p_payload, p_packet->length, &checksum, false);
        checksum = (uint16_t)~checksum;

        // Change pbuffer type.
        p_packet->type       = ICMP6_PACKET_TYPE;
        p_packet->p_payload  = p_packet->p_payload + ICMP6_HEADER_SIZE;
        p_packet->length    -= ICMP6_HEADER_SIZE;

        if (checksum != 0)
        {
            ICMP6_ERR("[ICMP6]: Bad checksum detected. Got 0x%08x but expected 0x%08x, 0x%08lX\r\n",
                      NTOHS(p_icmp_header->checksum), checksum, p_packet->length);
            process_result = ICMP6_BAD_CHECKSUM;
        }
        else
        {
            switch(p_icmp_header->type)
            {
                case ICMP6_TYPE_DESTINATION_UNREACHABLE:
                case ICMP6_TYPE_PACKET_TOO_LONG:
                case ICMP6_TYPE_TIME_EXCEED:
                case ICMP6_TYPE_PARAMETER_PROBLEM:
                    ICMP6_TRC("[ICMP6]: Got ICMPv6 error message with type = 0x%08x\r\n",
                              p_icmp_header->type);
                    break;
                case ICMP6_TYPE_ECHO_REQUEST:
                case ICMP6_TYPE_ECHO_REPLY:
                {
                    p_echo_header = (icmp6_echo_header_t *)p_packet->p_payload;

                    ICMP6_TRC("[ICMP6]: Got ICMPv6 Echo message with type = %x.\r\n",
                              p_icmp_header->type);
                    ICMP6_TRC("[ICMP6]: From:");
                    ICMP6_DUMP(p_ip_header->srcaddr.u8, IPV6_ADDR_SIZE);
                    ICMP6_TRC("[ICMP6]: Identifier: 0x%04x, Sequence Number: 0x%04x\r\n",
                              NTOHS(p_echo_header->identifier),
                              NTOHS(p_echo_header->sequence_number));
                    break;
                }
                case ICMP6_TYPE_ROUTER_SOLICITATION:
                case ICMP6_TYPE_ROUTER_ADVERTISEMENT:
                case ICMP6_TYPE_NEIGHBOR_SOLICITATION:
                case ICMP6_TYPE_NEIGHBOR_ADVERTISEMENT:
                    process_result = ndisc_input(p_interface,
                                                 p_ip_header,
                                                 p_icmp_header,
                                                 p_packet);
                    is_ndisc = true;
                    break;
                default:
                    process_result = ICMP6_UNHANDLED_PACKET_TYPE;
                    break;
            }
        }
    }

#if (ICMP6_ENABLE_ALL_MESSAGES_TO_APPLICATION == 1)

    err_code = app_notify_icmp_data(p_interface, p_packet, process_result);

#elif (ICMP6_ENABLE_ND6_MESSAGES_TO_APPLICATION == 1)

    if(is_ndisc)
    {
        err_code = app_notify_icmp_data(p_interface, p_packet, process_result);
    }

#endif

#if (ICMP6_ENABLE_HANDLE_ECHO_REQUEST_TO_APPLICATION == 0)
    if(p_icmp_header->type == ICMP6_TYPE_ECHO_REQUEST)
    {
        echo_reply_send(p_interface, p_ip_header, p_icmp_header, p_packet);
    }
#endif

    ICMP6_TRC("[ICMP6]: << icmp6_input\r\n");

    UNUSED_VARIABLE(is_ndisc);
    UNUSED_VARIABLE(p_echo_header);
    UNUSED_VARIABLE(process_result);

    SDK_MUTEX_UNLOCK();

    return err_code;
}
