zephyr_library_include_directories(
    ${ZEPHYR_CONFIG_DIR}/assets
    ${ZEPHYR_CONFIG_DIR}
)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ANALOG_INPUT src/analog_input.c)
zephyr_include_directories(include)

zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
