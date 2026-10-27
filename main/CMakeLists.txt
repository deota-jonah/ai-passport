idf_component_register(
    SRCS "agent_link_app/passport_main.cc"
         "agent_link_app/ai_passport_wifi.cc"
         "agent_link_app/passport_board.cc"
         "agent_link_app/passport_wifi_audio.cc"
         "agent_link_app/passport_wifi_ui.cc"
         "agent_link_app/passport_screen.cc"
         "agent_link_app/passport_language.cc"
         "agent_link_app/stripe_canvas.cc"
         "../assets/fonts/agent_link/raster_montserrat_14.c"
         "../assets/fonts/agent_link/raster_montserrat_20.c"
         "../assets/fonts/agent_link/raster_montserrat_28.c"
         "../assets/fonts/agent_link/raster_cjk_14.c"
    INCLUDE_DIRS "agent_link_app"
    REQUIRES bsp agent_link esp_lcd esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
