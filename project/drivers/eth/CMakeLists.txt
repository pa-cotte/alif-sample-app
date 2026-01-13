zephyr_library()

zephyr_library_property(ALLOW_EMPTY TRUE)

zephyr_library_sources_ifdef(CONFIG_ETH_ALIF eth_alif_phy.c eth_alif_mac.c)

zephyr_include_directories_ifdef(CONFIG_ETH_ALIF ${CMAKE_CURRENT_SOURCE_DIR}/../../soc)
