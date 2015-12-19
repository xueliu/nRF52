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

#include <string.h>
#include <stddef.h>

#include "lwm2m_objects.h"
#include "lwm2m.h"

//lint -e516 -save // Symbol '__INTADDR__()' has arg. type conflict 
#define LWM2M_INSTANCE_OFFSET_SET(instance, type) \
    instance->proto.operations_offset = offsetof(type, operations); \
    instance->proto.resource_ids_offset = offsetof(type, resource_ids); 
//lint -restore


void lwm2m_instance_security_init(lwm2m_security_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_security_t);

    p_instance->proto.object_id     = LWM2M_OBJ_SECURITY;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_security_t *)0)->operations);

    // Set access types.
    p_instance->operations[0]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[1]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[2]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[3]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[4]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[5]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[6]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[7]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[8]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[9]  = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[10] = LWM2M_OPERATION_CODE_NONE;
    p_instance->operations[11] = LWM2M_OPERATION_CODE_NONE;

    // Set resource IDs.
    p_instance->resource_ids[0]  = LWM2M_SECURITY_SERVER_URI;
    p_instance->resource_ids[1]  = LWM2M_SECURITY_BOOTSTRAP_SERVER;
    p_instance->resource_ids[2]  = LWM2M_SECURITY_SECURITY_MODE;
    p_instance->resource_ids[3]  = LWM2M_SECURITY_PUBLIC_KEY;
    p_instance->resource_ids[4]  = LWM2M_SECURITY_SERVER_PUBLIC_KEY;
    p_instance->resource_ids[5]  = LWM2M_SECURITY_SECRET_KEY;
    p_instance->resource_ids[6]  = LWM2M_SECURITY_SMS_SECURITY_MODE;
    p_instance->resource_ids[7]  = LWM2M_SECURITY_SMS_BINDING_KEY_PARAM;
    p_instance->resource_ids[8]  = LWM2M_SECURITY_SMS_BINDING_SECRET_KEY;
    p_instance->resource_ids[9]  = LWM2M_SECURITY_SERVER_SMS_NUMBER;
    p_instance->resource_ids[10] = LWM2M_SECURITY_SHORT_SERVER_ID;
    p_instance->resource_ids[11] = LWM2M_SECURITY_CLIENT_HOLD_OFF_TIME;
}


void lwm2m_instance_server_init(lwm2m_server_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_server_t);

    p_instance->proto.object_id     = LWM2M_OBJ_SERVER;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_server_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[1] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[2] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[3] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[4] = LWM2M_OPERATION_CODE_EXECUTE;
    p_instance->operations[5] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[6] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[7] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[8] = LWM2M_OPERATION_CODE_EXECUTE;
    
    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_SERVER_SHORT_SERVER_ID;
    p_instance->resource_ids[1] = LWM2M_SERVER_LIFETIME;
    p_instance->resource_ids[2] = LWM2M_SERVER_DEFAULT_MIN_PERIOD;
    p_instance->resource_ids[3] = LWM2M_SERVER_DEFAULT_MAX_PERIOD;
    p_instance->resource_ids[4] = LWM2M_SERVER_DISABLE;
    p_instance->resource_ids[5] = LWM2M_SERVER_DISABLE_TIMEOUT;
    p_instance->resource_ids[6] = LWM2M_SERVER_NOTIFY_WHEN_DISABLED;
    p_instance->resource_ids[7] = LWM2M_SERVER_BINDING;
    p_instance->resource_ids[8] = LWM2M_SERVER_REGISTRATION_UPDATE_TRIGGER;
}


void lwm2m_instance_firmware_init(lwm2m_firmware_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_firmware_t);

    p_instance->proto.object_id     = LWM2M_OBJ_FIRMWARE;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_firmware_t *)0)->operations);
   
    // Set access types.
    p_instance->operations[0] = LWM2M_OPERATION_CODE_WRITE;
    p_instance->operations[1] = LWM2M_OPERATION_CODE_WRITE;
    p_instance->operations[2] = LWM2M_OPERATION_CODE_EXECUTE;
    p_instance->operations[3] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[4] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[5] = LWM2M_OPERATION_CODE_READ;
    
    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_FIRMWARE_PACKAGE;
    p_instance->resource_ids[1] = LWM2M_FIRMWARE_PACKAGE_URI;
    p_instance->resource_ids[2] = LWM2M_FIRMWARE_UPDATE;
    p_instance->resource_ids[3] = LWM2M_FIRMWARE_STATE;
    p_instance->resource_ids[4] = LWM2M_FIRMWARE_UPDATE_SUPPORTED_OBJECTS;
    p_instance->resource_ids[5] = LWM2M_FIRMWARE_UPDATE_RESULT;
}


void lwm2m_instance_acl_init(lwm2m_acl_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_acl_t);

    p_instance->proto.object_id     = LWM2M_OBJ_ACL;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_acl_t *)0)->operations);
    
    // Set access types.
    p_instance->operations[0] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[1] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[2] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[3] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_ACL_OBJECT_ID;
    p_instance->resource_ids[1] = LWM2M_ACL_INSTANCE_ID;
    p_instance->resource_ids[2] = LWM2M_ACL_ACL;
    p_instance->resource_ids[3] = LWM2M_ACL_CONTROL_OWNER;
}


void lwm2m_instance_connectivity_monitoring_init(lwm2m_connectivity_monitoring_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_connectivity_monitoring_t);

    p_instance->proto.object_id     = LWM2M_OBJ_CONN_MON;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_connectivity_monitoring_t *)0)->operations);

    // Set access types.
    p_instance->operations[0]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[1]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[2]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[3]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[4]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[5]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[6]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[7]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[8]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[9]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[10] = LWM2M_OPERATION_CODE_READ;

    // Set resource IDs.
    p_instance->resource_ids[0]  = LWM2M_CONN_MON_NETWORK_BEARER;
    p_instance->resource_ids[1]  = LWM2M_CONN_MON_AVAILABLE_NETWORK_BEARER;
    p_instance->resource_ids[2]  = LWM2M_CONN_MON_RADIO_SIGNAL_STRENGHT;
    p_instance->resource_ids[3]  = LWM2M_CONN_MON_LINK_QUALITY;
    p_instance->resource_ids[4]  = LWM2M_CONN_MON_IP_ADDRESSES;
    p_instance->resource_ids[5]  = LWM2M_CONN_MON_ROUTER_IP_ADRESSES;
    p_instance->resource_ids[6]  = LWM2M_CONN_MON_LINK_UTILIZATION;
    p_instance->resource_ids[7]  = LWM2M_CONN_MON_APN;
    p_instance->resource_ids[8]  = LWM2M_CONN_MON_CELL_ID;
    p_instance->resource_ids[9]  = LWM2M_CONN_MON_SMNC;
    p_instance->resource_ids[10] = LWM2M_CONN_MON_SMCC;
}


void lwm2m_instance_connectivity_statistics_init(lwm2m_connectivity_statistics_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_connectivity_statistics_t);
    
    p_instance->proto.object_id     = LWM2M_OBJ_CONN_STAT;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_connectivity_statistics_t *)0)->operations);
    
    // Set access types.
    p_instance->operations[0] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[1] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[2] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[3] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[4] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[5] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[6] = LWM2M_OPERATION_CODE_EXECUTE;

    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_CONN_STAT_SMS_TX_COUNTER;
    p_instance->resource_ids[1] = LWM2M_CONN_STAT_SMS_RX_COUNTER;
    p_instance->resource_ids[2] = LWM2M_CONN_STAT_TX_DATA;
    p_instance->resource_ids[3] = LWM2M_CONN_STAT_RX_DATA;
    p_instance->resource_ids[4] = LWM2M_CONN_STAT_MAX_MSG_SIZE;
    p_instance->resource_ids[5] = LWM2M_CONN_STAT_AVG_MSG_SIZE;
    p_instance->resource_ids[6] = LWM2M_CONN_STAT_START_RESET;
}


void lwm2m_instance_device_init(lwm2m_device_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_device_t);

    p_instance->proto.object_id     = LWM2M_OBJ_DEVICE;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_device_t *)0)->operations);
        
    // Set access types.
    p_instance->operations[0]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[1]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[2]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[3]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[4]  = LWM2M_OPERATION_CODE_EXECUTE;
    p_instance->operations[5]  = LWM2M_OPERATION_CODE_EXECUTE;
    p_instance->operations[6]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[7]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[8]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[9]  = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[10] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[11] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[12] = LWM2M_OPERATION_CODE_EXECUTE;
    p_instance->operations[13] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[14] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[15] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    p_instance->operations[16] = LWM2M_OPERATION_CODE_READ;
    
    // Set resource IDs.
    p_instance->resource_ids[0]  = LWM2M_DEVICE_MANUFACTURER;
    p_instance->resource_ids[1]  = LWM2M_DEVICE_MODEL_NUMBER;
    p_instance->resource_ids[2]  = LWM2M_DEVICE_SERIAL_NUMBER;
    p_instance->resource_ids[3]  = LWM2M_DEVICE_FIRMWARE_VERSION;
    p_instance->resource_ids[4]  = LWM2M_DEVICE_REBOOT;
    p_instance->resource_ids[5]  = LWM2M_DEVICE_FACTORY_RESET;
    p_instance->resource_ids[6]  = LWM2M_DEVICE_AVAILABLE_POWER_SOURCES;
    p_instance->resource_ids[7]  = LWM2M_DEVICE_POWER_SOURCE_VOLTAGE;
    p_instance->resource_ids[8]  = LWM2M_DEVICE_POWER_SOURCE_CURRENT;
    p_instance->resource_ids[9]  = LWM2M_DEVICE_BATTERY_LEVEL;
    p_instance->resource_ids[10] = LWM2M_DEVICE_MEMORY_FREE;
    p_instance->resource_ids[11] = LWM2M_DEVICE_ERROR_CODE;
    p_instance->resource_ids[12] = LWM2M_DEVICE_RESET_ERROR_CODE;
    p_instance->resource_ids[13] = LWM2M_DEVICE_CURRENT_TIME;
    p_instance->resource_ids[14] = LWM2M_DEVICE_UTC_OFFSET;
    p_instance->resource_ids[15] = LWM2M_DEVICE_TIMEZONE;
    p_instance->resource_ids[16] = LWM2M_DEVICE_SUPPORTED_BINDINGS;
}


void lwm2m_instance_location_init(lwm2m_location_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_location_t);
    
    p_instance->proto.object_id     = LWM2M_OBJ_LOCATION;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_location_t *)0)->operations);

    // Set access types.
    p_instance->operations[0] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[1] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[2] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[3] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[4] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[5] = LWM2M_OPERATION_CODE_READ;
    
    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_LOCATION_ALTITUDE;
    p_instance->resource_ids[1] = LWM2M_LOCATION_LONGITUDE;
    p_instance->resource_ids[2] = LWM2M_LOCATION_ALTITUDE;
    p_instance->resource_ids[3] = LWM2M_LOCATION_UNCERTAINTY;
    p_instance->resource_ids[4] = LWM2M_LOCATION_VELOCITY;
    p_instance->resource_ids[5] = LWM2M_LOCATION_TIMESTAMP;
}


void lwm2m_instance_software_update_init(lwm2m_software_update_t * p_instance)
{
    // Set prototype variables.
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_software_update_t);
    
    p_instance->proto.object_id     = LWM2M_OBJ_SOFTWARE_UPDATE;
    p_instance->proto.instance_id   = 0;
    p_instance->proto.num_resources = sizeof(((lwm2m_software_update_t *)0)->operations);
    
    // Set access types.
    p_instance->operations[0] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[1] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[2] = LWM2M_OPERATION_CODE_WRITE;
    p_instance->operations[3] = LWM2M_OPERATION_CODE_WRITE;
    p_instance->operations[4] = LWM2M_OPERATION_CODE_EXECUTE;
    p_instance->operations[5] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[6] = LWM2M_OPERATION_CODE_EXECUTE;
    p_instance->operations[7] = LWM2M_OPERATION_CODE_READ;
    p_instance->operations[8] = (LWM2M_OPERATION_CODE_READ | LWM2M_OPERATION_CODE_WRITE);
    
    // Set resource IDs.
    p_instance->resource_ids[0] = LWM2M_SW_UPDATE_PKG_NAME;
    p_instance->resource_ids[1] = LWM2M_SW_UPDATE_PKG_VERSION;
    p_instance->resource_ids[2] = LWM2M_SW_UPDATE_PACKAGE;
    p_instance->resource_ids[3] = LWM2M_SW_UPDATE_PACKAGE_URI;
    p_instance->resource_ids[4] = LWM2M_SW_UPDATE_INSTALL;
    p_instance->resource_ids[5] = LWM2M_SW_UPDATE_CHECKPOINT;
    p_instance->resource_ids[6] = LWM2M_SW_UPDATE_UNINSTALL;
    p_instance->resource_ids[7] = LWM2M_SW_UPDATE_UPDATE_STATE;
    p_instance->resource_ids[8] = LWM2M_SW_UPDATE_SUPPORTED_OBJECTS;
}
