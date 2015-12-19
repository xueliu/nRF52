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
#include <string.h>
#include "ble_6lowpan.h"
#include "mem_manager.h"
#include "app_trace.h"
#include "sdk_os.h"
#include "sdk_config.h"
#include "iot_common.h"
#include "iot_context_manager.h"
#include "ipv6_api.h"
#include "icmp6_api.h"
#include "udp_api.h"
#include "icmp6.h"
#include "udp.h"

/**
 * @defgroup iot_sdk_ipv6 Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests. These are intended for debugging purposes and
 *          can be enabled by defining the IPV6_ENABLE_LOGS.
 * @note That if ENABLE_DEBUG_LOG_SUPPORT is disabled, having IPV6_ENABLE_LOGS has no effect.
 * @{
 */

#if (IPV6_ENABLE_LOGS == 1)

#define IPV6_TRC  app_trace_log                                                                     /**< Used for logging details. */
#define IPV6_ERR  app_trace_log                                                                     /**< Used for logging errors in the module. */
#define IPV6_DUMP app_trace_dump                                                                    /**< Used for dumping octet information to get details of bond information etc. */

#else // IPV6_ENABLE_LOGS

#define IPV6_TRC(...)                                                                               /**< Diasbles detailed logs. */
#define IPV6_ERR(...)                                                                               /**< Diasbles error logs. */
#define IPV6_DUMP(...)                                                                              /**< Diasbles dumping of octet streams. */

#endif // IPV6_ENABLE_LOGS


/**
 * @defgroup ipv6_mutex_lock_unlock Module's Mutex Lock/Unlock Macros.
 *
 * @details Macros used to lock and unlock modules. Currently, SDK does not use mutexes but
 *          framework is provided in case need arises to use an alternative architecture.
 * @{
 */
#define IPV6_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_ipv6_mutex)                                            /**< Lock module using mutex */
#define IPV6_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_ipv6_mutex)                                          /**< Unlock module using mutex */
/** @} */

/**
 * @defgroup api_param_check API Parameters check macros.
 *
 * @details Macros that verify parameters passed to the module in the APIs. These macros
 *          could be mapped to nothing in final versions of code to save execution and size.
 *          IPV6_DISABLE_API_PARAM_CHECK should be set to 0 to enable these checks.
 *
 * @{
 */

#if (IPV6_DISABLE_API_PARAM_CHECK == 0)

/**@brief Macro to check is module is initialized before requesting one of the module procedures. */
#define VERIFY_MODULE_IS_INITIALIZED()                                                             \
    if (m_event_handler == NULL)                                                                   \
    {                                                                                              \
        return (SDK_ERR_MODULE_NOT_INITIALZED | IOT_IPV6_ERR_BASE);                                \
    }

/**@brief Verify NULL parameters are not passed to API by application. */
#define NULL_PARAM_CHECK(PARAM)                                                                    \
    if ((PARAM) == NULL)                                                                           \
    {                                                                                              \
        return (NRF_ERROR_NULL | IOT_IPV6_ERR_BASE);                                               \
    }

#else // IPV6_DISABLE_API_PARAM_CHECK

#define VERIFY_MODULE_IS_INITIALIZED()
#define NULL_PARAM_CHECK(PARAM)

#endif // IPV6_DISABLE_API_PARAM_CHECK
/** @} */

#define PBUFFER_ICMP_PAYLOAD_OFFSET     IPV6_IP_HEADER_SIZE + ICMP6_HEADER_SIZE                     /**< ICMP payload offset. */
#define PBUFFER_UDP_PAYLOAD_OFFSET      IPV6_IP_HEADER_SIZE + UDP_HEADER_SIZE                       /**< UDP payload offset. */
#define PBUFFER_OTHER_PAYLOAD_OFFSET    IPV6_IP_HEADER_SIZE                                         /**< Raw IPv6 payload offset. */

#define IPV6_MAX_ADDRESS_COUNT         (IPV6_MAX_ADDRESS_PER_INTERFACE * IPV6_MAX_INTERFACE)        /**< Maximum number of addresses. */
#define IPV6_INVALID_ADDR_INDEX        0xFF                                                         /**< Invalid address representation. */

#define DEST_ADDR_OFFSET               24                                                           /**< Offset of destination address in IPv6 packet. */

/**@brief Internal interface structure. */
typedef struct
{
    iot_interface_t  * p_interface;                                                                 /**< Pointer to driver interface */
    uint8_t            addr_range[IPV6_MAX_ADDRESS_PER_INTERFACE];                                  /**< Indices to m_address_table indicating the address. If an index is IPV6_INVALID_ADDR_INDEX, it means there is no address entry. */
} interface_t;

/**@brief Application Event Handler. */
static ipv6_evt_handler_t m_event_handler = NULL;

/**@brief Table of addresses */
static ipv6_addr_conf_t m_address_table[IPV6_MAX_ADDRESS_COUNT];

/**@brief Network interfaces table. */
static interface_t m_interfaces[IPV6_MAX_INTERFACE];

/**@brief Number of network interfaces. */
static uint32_t m_interfaces_count = 0;

/**@brief Global address for IPv6 any. */
ipv6_addr_t ipv6_addr_any;

/**@brief Mutex variable. Currently unused, this declaration does not occupy any space in RAM. */
SDK_MUTEX_DEFINE(m_ipv6_mutex)


/**@brief Function for finding specific address in address table.
 *
 * @param[in]   p_addr   Checked address.
 * @param[out]  p_index  Index of address.
 *
 * @return  NRF_SUCCESS if success, NRF_ERROR_NOT_FOUND otherwise.
 */
static uint32_t addr_find(const ipv6_addr_t * p_addr, uint32_t * p_index)
{
    uint32_t index;
    uint32_t err_code = (IOT_IPV6_ERR_BASE | NRF_ERROR_NOT_FOUND);

    for (index = 0; index < IPV6_MAX_ADDRESS_COUNT; index++)
    {
        if ((m_address_table[index].state != IPV6_ADDR_STATE_UNUSED) &&
            (0 == IPV6_ADDRESS_CMP(&m_address_table[index].addr, p_addr)))
        {
            *p_index = index;
            err_code = NRF_SUCCESS;
            break;
        }
    }

    return err_code;
}

/**@brief Function for finding free place in address table.
 *
 * @param[out]  p_index   Index of address.
 *
 * @return  NRF_SUCCESS if success, NRF_ERROR_NOT_FOUND otherwise.
 */
static uint32_t addr_find_free(uint32_t * p_index)
{
    uint32_t index;
    uint32_t err_code = (IOT_IPV6_ERR_BASE | NRF_ERROR_NO_MEM);

    for (index = 0; index < IPV6_MAX_ADDRESS_COUNT; index++)
    {
        if (m_address_table[index].state == IPV6_ADDR_STATE_UNUSED)
        {
            *p_index = index;
            err_code = NRF_SUCCESS;
            break;
        }
    }

    return err_code;
}


/**@brief Function for freeing an address configuration entry in m_address_table.
 *
 * @param[in]   index             Index of address.
 * @param[in]   check_references  Indicate that before remove references should be counted.
 *
 * @return      None.
 */
static void addr_free(uint32_t addr_index, bool check_references)
{
    uint32_t if_index;
    uint32_t index;

    if(check_references)
    {
        for (if_index = 0; if_index < IPV6_MAX_INTERFACE; if_index++)
        {
            for (index = 0; index < IPV6_MAX_ADDRESS_PER_INTERFACE; index++)
            {
                if (m_interfaces[if_index].addr_range[index] == addr_index)
                {
                    return;
                }
            }
        }
    }

    m_address_table[addr_index].state = IPV6_ADDR_STATE_UNUSED;
    IPV6_ADDRESS_INITIALIZE(&m_address_table[addr_index].addr);
}


/**@brief Function for checking if received packet is for us.
 *        Currently only all-node multicast and MLDv2 are accepted.
 *
 * @param[in]   interface_id  Index of interface.
 * @param[in]   p_addr        Checked address.
 *
 * @return      NRF_SUCCESS if packet can be processing to IPv6 multiplexer.
 */
static uint32_t addr_check(uint32_t interface_id, ipv6_addr_t * p_addr)
{
    ipv6_addr_conf_t * p_addr_conf;
    uint32_t index;
    uint32_t err_code = NRF_ERROR_NOT_FOUND;

    for (index = 0; index < IPV6_MAX_ADDRESS_PER_INTERFACE; index++)
    {
        if (m_interfaces[interface_id].addr_range[index] != IPV6_INVALID_ADDR_INDEX)
        {
            p_addr_conf = &m_address_table[m_interfaces[interface_id].addr_range[index]];

            if ((0 == IPV6_ADDRESS_CMP(&p_addr_conf->addr, p_addr)) ||
                IPV6_ADDRESS_IS_MLDV2_MCAST(p_addr)                 ||
                IPV6_ADDRESS_IS_ALL_NODE(p_addr))
            {
                err_code = NRF_SUCCESS;
                break;
            }
        }
    }

    return err_code;
}


/**@brief Function for adding/updating IPv6 address in table.
 *
 * @param[in]   interface_id  Index of interface.
 * @param[in]   p_addr        Given address.
 *
 * @return      NRF_SUCCESS if operation successful, NRF_ERROR_NO_MEM otherwise.
 */
static uint32_t addr_set(const iot_interface_t   * p_interface,
                         const ipv6_addr_conf_t  * p_addr)
{
    uint32_t index;
    uint32_t addr_index;
    uint32_t err_code;

    uint32_t interface_id = (uint32_t)p_interface->p_upper_stack;

    // Try to find address.
    err_code = addr_find(&p_addr->addr, &addr_index);

    if(err_code != NRF_SUCCESS)
    {
        // Find first empty one.
        err_code = addr_find_free(&addr_index);
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = IOT_IPV6_ERR_ADDR_IF_MISMATCH;

        // Check if this index entry exists in the p_interface for which API is requested.
        for(index = 0; index < IPV6_MAX_ADDRESS_PER_INTERFACE; index++)
        {
            if (m_interfaces[interface_id].addr_range[index] == addr_index)
            {
                m_address_table[index].state = p_addr->state;

                err_code = NRF_SUCCESS;
                break;
            }
        }

        if (err_code == IOT_IPV6_ERR_ADDR_IF_MISMATCH)
        {
            err_code = (IOT_IPV6_ERR_BASE | NRF_ERROR_NO_MEM);

            for(index = 0; index < IPV6_MAX_ADDRESS_PER_INTERFACE; index++)
            {
                if (m_interfaces[interface_id].addr_range[index] == IPV6_INVALID_ADDR_INDEX)
                {
                    m_address_table[index].state = p_addr->state;
                    memcpy(&m_address_table[index].addr, p_addr, IPV6_ADDR_SIZE);
                    m_interfaces[interface_id].addr_range[index] = addr_index;

                    err_code = NRF_SUCCESS;
                    break;
                }
            }
        }
    }

    return err_code;
}


/**@brief Function for calculating how many bits of addresses are equal.
 *
 * @param[in]   p_addr1  Base address.
 * @param[in]   p_addr2  Base address.
 *
 * @return      Number of same bits.
 */
static uint32_t addr_bit_equal(const ipv6_addr_t * p_addr1,
                               const ipv6_addr_t * p_addr2)
{
    uint32_t index;
    uint32_t match = 0;
    uint8_t  temp;
    uint32_t index_tab;

    for (index = 0; index < IPV6_ADDR_SIZE; index++)
    {
        if (p_addr1->u8[index] == p_addr2->u8[index])
        {
            // Add full 8bits to match
            match += 8;
        }
        else
        {
            // Operation of XOR to detect differences
            temp = p_addr1->u8[index] ^ p_addr2->u8[index];

            // Check all single bits
            for (index_tab = 0; index_tab < 8; index_tab++)
            {
                if ((temp & 0x80) == 0)
                {
                    // If the oldest bits matched, add one more.
                    match++;

                    // Check next bit.
                    temp = temp << 1;
                }
                else
                {
                    break;
                }
            }

            break;
        }
    }

    return match;
}

/**@brief Function for searching specific network interface by given address.
 *
 * @param[in]   p_interface  Pointer to IPv6 network interface.
 * @param[in]   p_dest_addr  IPv6 address to be matched.
 *
 * @return      NRF_SUCCESS if operation successful, NRF_ERROR_NOT_FOUND otherwise.
 */
static uint32_t interface_find(iot_interface_t ** pp_interface, const ipv6_addr_t * p_dest_addr)
{
    // Currently only host role is implemented, though no need to match addresses.
    UNUSED_VARIABLE(p_dest_addr);

    uint32_t index;
    uint32_t err_code = (IOT_IPV6_ERR_BASE | NRF_ERROR_NOT_FOUND);

    if(m_interfaces_count == 1)
    {
        for (index = 0; index < IPV6_MAX_INTERFACE; index++)
        {
            if (m_interfaces[index].p_interface != NULL)
            {
                *pp_interface = m_interfaces[index].p_interface;
                err_code = NRF_SUCCESS;
                break;
            }
        }
    }
    else if (m_interfaces_count == 0)
    {
        err_code = (IOT_IPV6_ERR_BASE | NRF_ERROR_NOT_FOUND);
    }
    else
    {
        // Not supported now.
        err_code = (IOT_IPV6_ERR_BASE | NRF_ERROR_NOT_SUPPORTED);
    }

    return err_code;
}


/**@brief Function for resetting specific network interface.
 *
 * @param[in]   p_interface  Pointer to IPv6 network interface.
 *
 * @return      None.
 */
static void interface_reset(interface_t * p_interface)
{
    uint32_t index;
    uint8_t addr_index;

    p_interface->p_interface = NULL;

    for(index = 0; index < IPV6_MAX_ADDRESS_PER_INTERFACE; index++)
    {
        addr_index = p_interface->addr_range[index];

        if (addr_index != IPV6_INVALID_ADDR_INDEX)
        {
            p_interface->addr_range[index] = IPV6_INVALID_ADDR_INDEX;
            addr_free(index, true);
        }
    }
}


/**@brief Function for getting specific network interface by 6LoWPAN interface.
 *
 * @param[in]   p_6lo_interface  Pointer to 6LoWPAN interface.
 *
 * @return Pointer to internal network interface on success, otherwise NULL.
 */
static uint32_t interface_get_by_6lo(iot_interface_t * p_6lo_interface)
{
    return (uint32_t)(p_6lo_interface->p_upper_stack);
}


/**@brief Function for adding new 6lowpan interface to interface table.
 *
 * @param[in]    p_6lo_interface  Pointer to 6LoWPAN interface.
 * @param[out]   p_index          Pointer to index of internal network interface.
 *
 * @return      NRF_SUCCESS on success, otherwise NRF_ERROR_NO_MEM error.
 */
static uint32_t interface_add(iot_interface_t * p_interface,
                              uint32_t        * p_index )
{
    uint32_t         index;
    uint32_t         err_code;
    ipv6_addr_conf_t linklocal_addr;

    for (index = 0; index < IPV6_MAX_INTERFACE; index++)
    {
        if (m_interfaces[index].p_interface == NULL)
        {
            m_interfaces[index].p_interface = p_interface;
            p_interface->p_upper_stack      = (void *) index;
            (*p_index) = index;

            // Add link local address.
            IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&linklocal_addr.addr, p_interface->local_addr.identifier);
            linklocal_addr.state = IPV6_ADDR_STATE_PREFERRED;

            err_code = addr_set(p_interface, &linklocal_addr);

            if(err_code != NRF_SUCCESS)
            {
                IPV6_ERR("Cannot add link-local address to interface!\r\n");
            }

            return NRF_SUCCESS;
        }
    }

    return NRF_ERROR_NO_MEM;
}


/**@brief Function for removing 6lowpan interface from interface table.
 *
 * @param[in]   p_interface  Pointer to internal network interface.
 *
 * @return      None.
 */
static void interface_delete(uint32_t index)
{
    interface_reset(&m_interfaces[index]);
}

/**@brief Function for notifying application of the new interface established.
 *
 * @param[in]   p_interface  Pointer to internal network interface.
 *
 * @return      None.
 */
static void app_notify_interface_add(iot_interface_t * p_interface)
{
    ipv6_event_t event;

    event.event_id = IPV6_EVT_INTERFACE_ADD;

    IPV6_MUTEX_UNLOCK();

    m_event_handler(p_interface, &event);

    IPV6_MUTEX_LOCK();
}


/**@brief Function for notifying application of the interface disconnection.
 *
 * @param[in]   p_interface  Pointer to internal network interface.
 *
 * @return      None.
 */
static void app_notify_interface_delete(iot_interface_t * p_interface)
{
    ipv6_event_t event;

    event.event_id = IPV6_EVT_INTERFACE_DELETE;

    IPV6_MUTEX_UNLOCK();

    m_event_handler(p_interface, &event);

    IPV6_MUTEX_LOCK();
}


#if (IPV6_ENABLE_USNUPORTED_PROTOCOLS_TO_APPLICATION == 1)
/**@brief Function for notifying application of the received packet (e.g. with unsupported protocol).
 *
 * @param[in]   p_interface  Pointer to external interface from which packet come.
 * @param[in]   p_pbuffer    Pointer to packet buffer.
 *
 * @return      None.
 */
static void app_notify_rx_data(iot_interface_t * p_interface, iot_pbuffer_t * p_pbuffer)
{
    ipv6_event_t event;

    event.event_id = IPV6_EVT_INTERFACE_RX_DATA;

    // RX Event parameter.
    event.event_param.rx_event_param.p_rx_packet = p_pbuffer;
    event.event_param.rx_event_param.p_ip_header = (ipv6_header_t *)p_pbuffer->p_memory;

    IPV6_MUTEX_UNLOCK();

    m_event_handler(p_interface, &event);

    IPV6_MUTEX_LOCK();
}
#endif


/**@brief Function for multiplexing transport protocol to different modules.
 *
 * @param[in]   p_interface  Pointer to external interface from which packet come.
 * @param[in]   p_pbuffer    Pointer to packet buffer.
 *
 * @return      NRF_SUCCESS if success, otherwise an error code.
 */
static uint32_t ipv6_input(iot_interface_t * p_interface, iot_pbuffer_t * p_pbuffer)
{
    uint32_t        err_code = NRF_SUCCESS;
    ipv6_header_t * p_iphdr = (ipv6_header_t *)(p_pbuffer->p_payload - IPV6_IP_HEADER_SIZE);

    // Change byte order of IP header given to application.
    p_iphdr->length    = NTOHS(p_iphdr->length);
    p_iphdr->flowlabel = NTOHS(p_iphdr->flowlabel);

    switch(p_iphdr->next_header)
    {
        case IPV6_NEXT_HEADER_ICMP6:
            IPV6_TRC("[IPV6]: Got ICMPv6 packet\r\n");

            IPV6_MUTEX_UNLOCK();
            err_code = icmp6_input(p_interface, p_iphdr, p_pbuffer);
            IPV6_MUTEX_LOCK();

            break;

        case IPV6_NEXT_HEADER_UDP:
            IPV6_TRC("[IPV6]: Got UDP packet.\r\n");

            IPV6_MUTEX_UNLOCK();
            err_code = udp_input(p_interface, p_iphdr, p_pbuffer);
            IPV6_MUTEX_LOCK();

           break;

        default:
            IPV6_TRC("[IPV6]: Got unsupported protocol packet. Protocol ID = 0x%x!\r\n",
                     p_iphdr->next_header);

#if (IPV6_ENABLE_USNUPORTED_PROTOCOLS_TO_APPLICATION == 1)
            app_notify_rx_data(p_interface, p_pbuffer);
#endif
            break;
    }

    // Free packet buffer unless marked explicitly as pending
    if (err_code != IOT_IPV6_ERR_PENDING)
    {
        UNUSED_VARIABLE(iot_pbuffer_free(p_pbuffer, true));
    }

    return err_code;
}


/**@brief Function for receiving 6LoWPAN module events.
 *
 * @param[in]   p_6lo_interface  Pointer to 6LoWPAN interface.
 * @param[in]   p_6lo_event      Pointer to 6LoWPAN related event.
 *
 * @return      None.
 */
static void ble_6lowpan_evt_handler(iot_interface_t     * p_interface,
                                    ble_6lowpan_event_t * p_6lo_event)
{
    bool                        rx_failure = false;
    uint32_t                    err_code;
    uint32_t                    interface_id;
    iot_pbuffer_t             * p_pbuffer;
    iot_pbuffer_alloc_param_t   pbuff_param;

    IPV6_MUTEX_LOCK();

    IPV6_TRC("[IPV6]: >> ble_6lowpan_evt_handler\r\n");

    IPV6_TRC("[IPV6]: In 6LoWPAN Handler:\r\n");

    interface_id = interface_get_by_6lo(p_interface);

    switch(p_6lo_event->event_id)
    {
        case BLE_6LO_EVT_ERROR:
        {
            IPV6_TRC("[IPV6]: Got error, with result %08lx\r\n", p_6lo_event->event_result);
            break;
        }
        case BLE_6LO_EVT_INTERFACE_ADD:
        {
            IPV6_TRC("[IPV6]: New interface established!\r\n");

            // Add interface to internal table.
            err_code = interface_add(p_interface, &interface_id);

            if(NRF_SUCCESS == err_code)
            {
                IPV6_TRC("[IPV6]: Added new network interface to internal table.\r\n");

                err_code = iot_context_manager_table_alloc(p_interface);

                if(err_code == NRF_SUCCESS)
                {
                    IPV6_TRC("[IPV6]: Successfully allocated context table!\r\n");
                }
                else
                {
                    IPV6_TRC("[IPV6]: Failed to allocate context table!\r\n");
                }

                // Increase number of up interfaces.
                m_interfaces_count++;

                // Notify application.
                app_notify_interface_add(p_interface);
            }
            else
            {
                IPV6_ERR("[IPV6]: Cannot add new interface. Table is full.\r\n");
            }

            break;
        }
        case BLE_6LO_EVT_INTERFACE_DELETE:
        {
            IPV6_TRC("[IPV6]: Interface disconnected!\r\n");

            if (interface_id < IPV6_MAX_INTERFACE)
            {
                IPV6_TRC("[IPV6]: Removed network interface.\r\n");

                // Notify application.
                app_notify_interface_delete(p_interface);

                err_code = iot_context_manager_table_free(p_interface);

                if(err_code == NRF_SUCCESS)
                {
                    IPV6_TRC("[IPV6_TRC]: Successfully freed context table!\r\n");
                }

                // Decrease number of up interfaces.
                m_interfaces_count--;

                // Remove interface from internal table.
                interface_delete(interface_id);
            }
            break;
        }
        case BLE_6LO_EVT_INTERFACE_DATA_RX:
        {
            IPV6_TRC("[IPV6]: Got data with size = %d!\r\n",
                     p_6lo_event->event_param.rx_event_param.packet_len);
            IPV6_TRC("[IPV6]: Data: \r\n");
            IPV6_DUMP(p_6lo_event->event_param.rx_event_param.p_packet,
                      p_6lo_event->event_param.rx_event_param.packet_len);
            IPV6_TRC("\r\n");

            if (interface_id < IPV6_MAX_INTERFACE)
            {
                if(p_6lo_event->event_result == NRF_ERROR_NOT_FOUND)
                {
                    IPV6_ERR("[IPV6]: Cannot restore IPv6 addresses!");
                    IPV6_ERR("[IPV6]: Source CID = 0x%x, Destination CID = 0x%x\r\n",
                             p_6lo_event->event_param.rx_event_param.rx_contexts.src_cntxt_id,
                             p_6lo_event->event_param.rx_event_param.rx_contexts.dest_cntxt_id);

                    // Indicates failure.
                    rx_failure = true;
                    break;
                }

                // Check if packet is for us.
                ipv6_addr_t * p_addr =
                  (ipv6_addr_t *)&p_6lo_event->event_param.rx_event_param.p_packet[DEST_ADDR_OFFSET];

                err_code = addr_check(interface_id, p_addr);

                // If no address found - drop message.
                if(err_code != NRF_SUCCESS)
                {
                    IPV6_ERR("[IPV6]: Packet received on unknown address!\r\n");
                    rx_failure = true;
                    break;
                }

                // Try to allocate pbuffer, with no memory.
                pbuff_param.flags  = PBUFFER_FLAG_NO_MEM_ALLOCATION;
                pbuff_param.type   = RAW_PACKET_TYPE;
                pbuff_param.length = p_6lo_event->event_param.rx_event_param.packet_len;

                // Try to allocate pbuffer for receiving data.
                err_code = iot_pbuffer_allocate(&pbuff_param, &p_pbuffer);

                if(err_code == NRF_SUCCESS)
                {
                    p_pbuffer->p_memory  = p_6lo_event->event_param.rx_event_param.p_packet;
                    p_pbuffer->p_payload = p_pbuffer->p_memory + IPV6_IP_HEADER_SIZE;
                    p_pbuffer->length   -= IPV6_IP_HEADER_SIZE;

                    // Execute multiplexer.
                    err_code = ipv6_input(p_interface, p_pbuffer);

                    if(err_code != NRF_SUCCESS)
                    {
                        IPV6_ERR("[IPV6]: Failed while processing packet, error = 0x%08lX!\r\n",
                                 err_code);
                    }
                }
                else
                {
                    IPV6_ERR("[IPV6]: Failed to allocate packet buffer!\r\n");

                    rx_failure = true;
                }
            }
            else
            {
                IPV6_ERR("[6LOWPAN]: Got data to unknown interface!\r\n");

                rx_failure = true;
            }

            break;
        }
        default:
            break;
    }

    if(rx_failure == true)
    {
        UNUSED_VARIABLE(nrf_free(p_6lo_event->event_param.rx_event_param.p_packet));
    }

    IPV6_TRC("[IPV6]: << ble_6lowpan_evt_handler\r\n");

    IPV6_MUTEX_UNLOCK();
}


uint32_t ipv6_init(const ipv6_init_t * p_init)
{
    uint32_t           index;
    uint32_t           err_code;
    ble_6lowpan_init_t init_params;

    NULL_PARAM_CHECK(p_init);
    NULL_PARAM_CHECK(p_init->p_eui64);
    NULL_PARAM_CHECK(p_init->event_handler);

    SDK_MUTEX_INIT(m_ipv6_mutex);
    IPV6_MUTEX_LOCK();

    IPV6_TRC("[IPV6]: >> ipv6_init\r\n");

    // Initialize related modules.
    UNUSED_VARIABLE(nrf_mem_init());
    UNUSED_VARIABLE(iot_pbuffer_init());

    // Initialize submodules of IPv6 stack.
    UNUSED_VARIABLE(udp_init());
    UNUSED_VARIABLE(icmp6_init());

    // Initialize context manager.
    UNUSED_VARIABLE(iot_context_manager_init());

    IPV6_ADDRESS_INITIALIZE(IPV6_ADDR_ANY);

    // Set application event handler.
    m_event_handler = p_init->event_handler;

    // Clear number of interfaces.
    m_interfaces_count = 0;

    // Clear network interfaces.
    for(index = 0; index < IPV6_MAX_INTERFACE; index++)
    {
        interface_reset(&m_interfaces[index]);
    }

    // Clear all addresses.
    for(index = 0; index < IPV6_MAX_ADDRESS_COUNT; index++)
    {
        addr_free(index, false);
    }

    // 6LoWPAN module initialization.
    init_params.p_eui64       = p_init->p_eui64;
    init_params.event_handler = ble_6lowpan_evt_handler;

    err_code = ble_6lowpan_init(&init_params);

    IPV6_TRC("[IPV6]: << ipv6_init\r\n");

    IPV6_MUTEX_UNLOCK();

    return err_code;
}


uint32_t ipv6_address_set(const iot_interface_t  * p_interface,
                          const ipv6_addr_conf_t * p_addr)
{
    VERIFY_MODULE_IS_INITIALIZED();

    NULL_PARAM_CHECK(p_addr);
    NULL_PARAM_CHECK(p_interface);

    uint32_t err_code;

    IPV6_MUTEX_LOCK();

    IPV6_TRC("[IPV6]: >> ipv6_address_set\r\n");

    err_code = addr_set(p_interface, p_addr);

    IPV6_TRC("[IPV6]: << ipv6_address_set\r\n");

    IPV6_MUTEX_UNLOCK();

    return err_code;
}

uint32_t ipv6_address_find_best_match(iot_interface_t   ** pp_interface,
                                      ipv6_addr_t       *  p_addr_r,
                                      const ipv6_addr_t *  p_addr_f)
{
    VERIFY_MODULE_IS_INITIALIZED();

    NULL_PARAM_CHECK(p_addr_f);
    NULL_PARAM_CHECK(pp_interface);

    uint32_t      index;
    uint32_t      err_code;
    uint32_t      addr_index;
    uint32_t      match_temp  = 0;
    uint32_t      match_best  = 0;
    ipv6_addr_t * p_best_addr = NULL;

    IPV6_MUTEX_LOCK();

    err_code = interface_find(pp_interface, p_addr_f);

    if(err_code == NRF_SUCCESS && p_addr_r)
    {
        uint32_t interface_id = (uint32_t)(*pp_interface)->p_upper_stack;

        for(index = 0; index < IPV6_MAX_ADDRESS_PER_INTERFACE; index++)
        {
            addr_index = m_interfaces[interface_id].addr_range[index];

            if(addr_index != IPV6_INVALID_ADDR_INDEX)
            {
                if(m_address_table[addr_index].state == IPV6_ADDR_STATE_PREFERRED)
                {
                    match_temp = addr_bit_equal(p_addr_f, &m_address_table[addr_index].addr);

                    if(match_temp >= match_best)
                    {
                        match_best  = match_temp;
                        p_best_addr = &m_address_table[addr_index].addr;
                    }
                }
            }
        }

        // No address found.
        if(p_best_addr == NULL)
        {
            // Set undefined :: address.
            IPV6_ADDRESS_INITIALIZE(p_addr_r);
        }
        else
        {
            memcpy(p_addr_r->u8, p_best_addr->u8, IPV6_ADDR_SIZE);
        }
    }

    IPV6_MUTEX_UNLOCK();

    return err_code;
}


uint32_t ipv6_address_remove(const iot_interface_t * p_interface,
                             const ipv6_addr_t     * p_addr)
{
    VERIFY_MODULE_IS_INITIALIZED();

    NULL_PARAM_CHECK(p_addr);
    NULL_PARAM_CHECK(p_interface);

    uint32_t    index;
    uint32_t    err_code;
    uint32_t    addr_index;

    IPV6_MUTEX_LOCK();

    IPV6_TRC("[IPV6]: >> ipv6_address_remove\r\n");

    uint32_t interface_id = (uint32_t)p_interface->p_upper_stack;

    err_code = (IOT_IPV6_ERR_BASE | NRF_ERROR_NOT_FOUND);

    for (index = 0; index < IPV6_MAX_ADDRESS_PER_INTERFACE; index++)
    {
        addr_index = m_interfaces[interface_id].addr_range[index];

        if (addr_index != IPV6_INVALID_ADDR_INDEX)
        {
            if (0 == IPV6_ADDRESS_CMP(&m_address_table[addr_index].addr, p_addr))
            {
                m_interfaces[interface_id].addr_range[index] = IPV6_INVALID_ADDR_INDEX;

                // Remove address if no reference to interface found.
                addr_free(index, true);

                err_code = NRF_SUCCESS;

                break;
            }
        }
    }

    IPV6_TRC("[IPV6]: << ipv6_address_remove\r\n");

    IPV6_MUTEX_UNLOCK();

    return err_code;
}


uint32_t ipv6_send(const iot_interface_t * p_interface, iot_pbuffer_t * p_packet)
{
    VERIFY_MODULE_IS_INITIALIZED();

    NULL_PARAM_CHECK(p_packet);
    NULL_PARAM_CHECK(p_interface);

    uint32_t err_code;

    IPV6_MUTEX_LOCK();

    IPV6_TRC("[IPV6]: >> ipv6_send\r\n");

    err_code = ble_6lowpan_interface_send(p_interface,
                                          p_packet->p_payload,
                                          p_packet->length);

    if(err_code != NRF_SUCCESS)
    {
        IPV6_TRC("[IPV6]: Cannot send packet!\r\n");
    }

    // Free pbuffer, without freeing memory.
    UNUSED_VARIABLE(iot_pbuffer_free(p_packet, false));

    IPV6_TRC("[IPV6]: << ipv6_send\r\n");

    IPV6_MUTEX_UNLOCK();

    return err_code;
}
