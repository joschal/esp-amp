idf_component_register(SRCS
        "src/amp_addressing.c"
        "src/main.c"
        "src/mesh_internals.c"
        "src/amp_msg_handler.c"
        "src/amp_routing.c"
        "src/amp_outbound.c"
        "src/amp_serial_inbound.c"
        "src/amp_esp_inbound.c"
        "src/business_logic.c"
        "src/gpio_for_bl.c"

        INCLUDE_DIRS
        "."
        "include")


