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

    zephyr_library_include_directories(${ZMK_CONFIG}/assets)
    file(GLOB_RECURSE ASSET_SOURCES "${ZMK_CONFIG}/assets/*.c")
    zephyr_sources(${ASSET_SOURCES})


    if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
      zephyr_library_sources(widgets/screen.c)
    else()
      zephyr_library_sources(widgets/screen.c)
    endif()
endif()

