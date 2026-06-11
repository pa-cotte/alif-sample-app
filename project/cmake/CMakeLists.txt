# Confidential - Copyright PA.COTTE: All rights reserved
#
# Register JLink as an additional flash runner for Alif E7 boards.

if(CONFIG_SOC_AE722F80F55D5XX_RTSS_HP)
    board_runner_args(jlink "--device=AE722F80F55D5_HP")
elseif(CONFIG_SOC_AE722F80F55D5XX_RTSS_HE)
    board_runner_args(jlink "--device=AE722F80F55D5_HE")
endif()

board_finalize_runner_args(jlink "--dt-flash=y")
