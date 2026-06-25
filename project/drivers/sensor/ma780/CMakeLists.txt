# Confidential - Copyright PA.COTTE: All rights reserved

zephyr_library()

zephyr_library_sources(ma780.c)
zephyr_library_sources_ifdef(CONFIG_MA780_TRIGGER ma780_trigger.c)
