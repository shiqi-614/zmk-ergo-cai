if(CONFIG_ZMK_DISPLAY AND CONFIG_EPAPER_GDEY0213B74)

    zephyr_library()
    zephyr_library_sources(${ZEPHYR_BASE}/misc/empty_file.c)
    zephyr_library_include_directories(${ZEPHYR_LVGL_MODULE_DIR})
    zephyr_library_include_directories(${ZEPHYR_BASE}/lib/gui/lvgl/)
    zephyr_library_include_directories(${ZEPHYR_BASE}/drivers)
    zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
    zephyr_library_include_directories(${ZEPHYR_CURRENT_MODULE_DIR}/include)
    zephyr_library_include_directories(${ZEPHYR_CURRENT_CMAKE_DIR}/include)
    zephyr_library_include_directories(include)


    zephyr_library_sources(custom_status_screen.c)
    zephyr_library_sources(assets/cat/charging.c)
    zephyr_library_sources(assets/cat/connecting.c)
    zephyr_library_sources(assets/cat/pairing.c)
    zephyr_library_sources(assets/cat/running.c)
    zephyr_library_sources(assets/cat/sleeping.c)

    zephyr_library_sources(widgets/battery.c)
    zephyr_library_sources(widgets/cat_status.c)
    zephyr_library_sources(widgets/info_panel.c)
    zephyr_library_sources(widgets/layer.c)
    zephyr_library_sources(widgets/profile.c)
    zephyr_library_sources(widgets/role.c)
    zephyr_library_sources(widgets/screen.c)
    zephyr_library_sources(widgets/split.c)

    # zephyr_library_include_directories(${ZMK_CONFIG}/assets)
    # file(GLOB_RECURSE ASSET_SOURCES "${ZMK_CONFIG}/assets/*.c")
    # zephyr_sources(${ASSET_SOURCES})

    # zephyr_library_include_directories(${ZMK_CONFIG}/widgets)
    # file(GLOB WIDGET_SOURCES "${ZMK_CONFIG}/widgets/*.c")
    # zephyr_sources(${WIDGET_SOURCES})

endif()

