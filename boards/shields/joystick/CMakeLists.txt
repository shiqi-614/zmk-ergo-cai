if(CONFIG_JOYSTICK)
    zephyr_library()
    zephyr_library_sources(analog_input.c)
    zephyr_include_directories(include)

endif()

