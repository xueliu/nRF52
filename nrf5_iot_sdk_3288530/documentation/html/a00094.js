var a00094 =
[
    [ "Context Manager", "a00014.html", [
      [ "6LoWPAN layer", "a00014.html#IOT_CM_ARCH_1", null ],
      [ "IPv6 layer", "a00014.html#IOT_CM_ARCH_2", null ],
      [ "Configuration parameters", "a00014.html#IOT_MEM_MANAGER_CONF", [
        [ "IOT_CONTEXT_MANAGER_DISABLE_LOG", "a00014.html#IOT_CONTEXT_MANAGER_DISABLE_LOG", null ],
        [ "IOT_CONTEXT_MANAGER_DISABLE_API_PARAM_CHECK", "a00014.html#IOT_CONTEXT_MANAGER_DISABLE_API_PARAM_CHECK", null ],
        [ "IOT_CONTEXT_MANAGER_MAX_CONTEXTS", "a00014.html#IOT_CONTEXT_MANAGER_MAX_CONTEXTS", null ],
        [ "IOT_CONTEXT_MANAGER_MAX_TABLES", "a00014.html#IOT_CONTEXT_MANAGER_MAX_TABLES", null ]
      ] ]
    ] ],
    [ "6LoWPAN over BLE", "a00011.html", [
      [ "6LoWPAN interface", "a00011.html#IOT_6LO_INTERFACE", null ],
      [ "API functions", "a00011.html#IOT_6LO_API", [
        [ "Initialization/setup procedure", "a00011.html#IOT_6LO_API_1", null ],
        [ "Send procedure", "a00011.html#IOT_6LO_API_2", null ],
        [ "Asynchronous Event Notification Callback", "a00011.html#IOT_6LO_CALLBACK", null ]
      ] ]
    ] ],
    [ "Packet Buffer", "a00018.html", [
      [ "Allocating the packet buffer", "a00018.html#IOT_PBUFFER_OVERVIEW_1", null ],
      [ "Reallocating the packet buffer", "a00018.html#IOT_PBUFFER_OVERVIEW_2", null ],
      [ "Freeing the packet buffer", "a00018.html#IOT_PBUFFER_OVERVIEW_3", null ],
      [ "Code examples", "a00018.html#IOT_PBUFFER_CODE", [
        [ "Allocating CoAP packet, with 100 bytes payload", "a00018.html#IOT_PBUFFER_CODE_1", null ],
        [ "Reallocating the UDP packet buffer to 10 bytes more", "a00018.html#IOT_PBUFFER_CODE_2", null ]
      ] ],
      [ "Configuration parameters", "a00018.html#IOT_PBUFFER_CONF", [
        [ "IOT_PBUFFER_DISABLE_LOG", "a00018.html#IOT_PBUFFER_DISABLE_LOG", null ],
        [ "IOT_PBUFFER_DISABLE_API_PARAM_CHECK", "a00018.html#IOT_PBUFFER_DISABLE_API_PARAM_CHECK", null ],
        [ "IOT_PBUFFER_MAX_COUNT", "a00018.html#IOT_PBUFFER_MAX_COUNT", null ]
      ] ]
    ] ],
    [ "Commissioning", "a00013.html", "a00013" ],
    [ "BSD Socket Interface", "a00020.html", [
      [ "Blocking/Non-Blocking behavior", "a00020.html#SOCKET_BLOCKORNOT", [
        [ "Blocking I/O", "a00020.html#IOT_SOCKET_BLOCKING", null ],
        [ "Non-Blocking I/O", "a00020.html#IOT_SOCKET_NONBLOCKING", null ]
      ] ],
      [ "Configuration parameters", "a00020.html#IOT_SOCKET_CONFIGURATION", null ],
      [ "References", "a00020.html#IOT_SOCKET_REFERENCES", null ]
    ] ],
    [ "IoT Timer", "a00029.html", [
      [ "Code examples", "a00029.html#IOT_TIMER_CODE", [
        [ "Initializing an application timer as a tick source for the IoT Timer", "a00029.html#IOT_TIMER_CODE_1", null ],
        [ "Configuring clients for the IoT Timer", "a00029.html#IOT_TIMER_CODE_2", null ],
        [ "Querying the IoT Timer for the wall clock value", "a00029.html#IOT_TIMER_CODE_3", null ]
      ] ],
      [ "Configuration parameters", "a00029.html#IOT_TIMER_CONF", [
        [ "IOT_TIMER_RESOLUTION_IN_MS", "a00029.html#IOT_TIMER_RESOLUTION_IN_MS", null ],
        [ "IOT_TIMER_DISABLE_API_PARAM_CHECK", "a00029.html#IOT_TIMER_DISABLE_API_PARAM_CHECK", null ]
      ] ],
      [ "Implementation specific limitations", "a00029.html#IOT_TIMER_LIMITATIONS", null ]
    ] ],
    [ "IoT File", "a00016.html", [
      [ "Static memory access", "a00016.html#iot_file_static", null ],
      [ "Flash memory access", "a00016.html#iot_file_pstorage_raw", null ],
      [ "Configuration parameters", "a00016.html#IOT_FILE_CONF", [
        [ "IOT_FILE_DISABLE_LOGS", "a00016.html#IOT_FILE_DISABLE_LOGS", null ],
        [ "IOT_FILE_DISABLE_API_PARAM_CHECK", "a00016.html#IOT_FILE_DISABLE_API_PARAM_CHECK", null ]
      ] ],
      [ "Specifics and limitations", "a00016.html#IOT_FILE_LIMITATIONS", [
        [ "Implemented features", "a00016.html#IOT_FILE_LIMITATION_Features", null ],
        [ "Limitations", "a00016.html#IOT_FILE_LIMITATION_limitations", null ]
      ] ]
    ] ],
    [ "IoT DFU", "a00015.html", "a00015" ],
    [ "CoAP", "a00012.html", "a00012" ],
    [ "LWM2M", "a00017.html", [
      [ "LWM2M and IPSO", "a00017.html#lib_iot_lwm2m_lwm2m_and_ipso", null ],
      [ "Objects", "a00017.html#lib_iot_lwm2m_resource_objects", null ],
      [ "Instances and resources", "a00017.html#lwm2m_resource_instances_and_resources", [
        [ "Disable Resource ID", "a00017.html#lwm2m_resource_instance_and_resources_disable_member", null ]
      ] ],
      [ "LWM2M CoAP integration", "a00017.html#lib_iot_lwm2m_coap", null ],
      [ "LWM2M API", "a00017.html#lib_iot_lwm2m_api", [
        [ "Initialization", "a00017.html#lib_iot_lwm2m_api_init", null ],
        [ "Notifications", "a00017.html#lib_iot_lwm2m_api_notification", null ],
        [ "Root handler", "a00017.html#lib_iot_lwm2m_api_root", null ],
        [ "Object initialization", "a00017.html#lib_iot_lwm2m_api_object_initialization", null ],
        [ "Instance initialization", "a00017.html#lib_iot_lwm2m_api_instance_initialization", null ],
        [ "Named Objects", "a00017.html#lib_iot_lwm2m_api_name_objects", null ],
        [ "Add an object", "a00017.html#lib_iot_lwm2m_api_object_add", null ],
        [ "Delete an object", "a00017.html#lib_iot_lwm2m_api_object_delete", null ],
        [ "Add an instance", "a00017.html#lib_iot_lwm2m_api_instance_add", null ],
        [ "Delete an instance", "a00017.html#lib_iot_lwm2m_api_instance_delete", null ],
        [ "Generate a Link Format string", "a00017.html#lib_iot_lwm2m_api_gen_link_format", null ],
        [ "Encoding TLV", "a00017.html#lib_iot_lwm2m_api_tlv_encoding", null ],
        [ "Decoding TLV", "a00017.html#lib_iot_lwm2m_api_tlv_decoding", null ],
        [ "Respond with payload", "a00017.html#lib_iot_lwm2m_api_respond_with_payload", null ],
        [ "Respond with code", "a00017.html#lib_iot_lwm2m_api_respond_with_code", null ],
        [ "Initiate bootstrap", "a00017.html#lib_iot_lwm2m_api_bootstrap", null ],
        [ "Register", "a00017.html#lib_iot_lwm2m_api_register", null ],
        [ "Registration update", "a00017.html#lib_iot_lwm2m_api_update", null ],
        [ "Deregister", "a00017.html#lib_iot_lwm2m_api_deregister", null ]
      ] ],
      [ "Features", "a00017.html#IOT_LWM2M_features", null ],
      [ "Configuration parameters", "a00017.html#IOT_LWM2M_CONF", [
        [ "LWM2M_DISABLE_LOGS", "a00017.html#LWM2M_DISABLE_LOGS", null ],
        [ "LWM2M_DISABLE_API_PARAM_CHECK", "a00017.html#LWM2M_DISABLE_API_PARAM_CHECK", null ],
        [ "LWM2M_MAX_SERVERS", "a00017.html#LWM2M_MAX_SERVERS", null ],
        [ "LWM2M_COAP_HANDLER_MAX_OBJECTS", "a00017.html#LWM2M_COAP_HANDLER_MAX_OBJECTS", null ],
        [ "LWM2M_COAP_HANDLER_MAX_INSTANCES", "a00017.html#LWM2M_COAP_HANDLER_MAX_INSTANCES", null ],
        [ "LWM2M_REGISTER_MAX_LOCATION_LEN", "a00017.html#LWM2M_REGISTER_MAX_LOCATION_LEN", null ]
      ] ]
    ] ],
    [ "Nordic's IPv6 stack", "a00021.html", "a00021" ],
    [ "Transport Layer Security on nRF5x", "a00019.html", [
      [ "Overview", "a00019.html#lib_iot_security_overview", null ],
      [ "Nordic's TLS Abstraction Interface", "a00019.html#lib_iot_security_interface", [
        [ "Feature Configuration Summary", "a00019.html#lib_iot_security_config_table", null ],
        [ "Application Interface", "a00019.html#lib_iot_security_api", null ]
      ] ],
      [ "Version and Reference", "a00019.html#mbedtls", null ]
    ] ],
    [ "lwIP stack on nRF5x", "a00083.html", [
      [ "lwIP stack driver", "a00083.html#iot_sdk_lwip_stack_driver", null ],
      [ "lwIP version and reference", "a00083.html#iot_sdk_lwip_reference", null ]
    ] ]
];