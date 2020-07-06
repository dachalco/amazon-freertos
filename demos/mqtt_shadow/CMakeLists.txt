# C SDK MQTT demo
afr_demo_module(mqtt_shadow)

afr_set_demo_metadata(ID "MQTT_SHADOW_DEMO")
afr_set_demo_metadata(DESCRIPTION "An example that demonstrates MQTT with SHADOW on shared connection")
afr_set_demo_metadata(DISPLAY_NAME "MQTT+SHADOW Hello World")

afr_module_sources(
    ${AFR_CURRENT_MODULE}
    INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/iot_demo_mqtt_shadow.c"
)
afr_module_dependencies(
    ${AFR_CURRENT_MODULE}
    INTERFACE
        AFR::mqtt
        AFR::shadow
)

afr_module_include_dirs(
        ${AFR_CURRENT_MODULE}
        INTERFACE
            "${AFR_MODULES_C_SDK_DIR}/standard/serializer/include"
)



