cmake_minimum_required(VERSION 4.0)
project(IAP_Main C)

set(CMAKE_C_STANDARD 11)

include_directories(Inc)

add_executable(IAP_Main
        Inc/common.h
        Inc/flash_if.h
        Inc/main.h
        Inc/menu.h
        Inc/stm32l4xx_hal_conf.h
        Inc/stm32l4xx_it.h
        Inc/ymodem.h
        Src/common.c
        Src/flash_if.c
        Src/main.c
        Src/menu.c
        Src/stm32l4xx_it.c
        Src/system_stm32l4xx.c
        Src/ymodem.c)
