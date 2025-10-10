################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/BSP/bsp_audio.c \
../Drivers/BSP/bsp_misc.c 

OBJS += \
./Drivers/BSP/bsp_audio.o \
./Drivers/BSP/bsp_misc.o 

C_DEPS += \
./Drivers/BSP/bsp_audio.d \
./Drivers/BSP/bsp_misc.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/BSP/%.o Drivers/BSP/%.su Drivers/BSP/%.cyclo: ../Drivers/BSP/%.c Drivers/BSP/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/AUDIO/Inc -I"C:/ST/DefaultWorkspase/soundcard_v1/Drivers/BSP" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-BSP

clean-Drivers-2f-BSP:
	-$(RM) ./Drivers/BSP/bsp_audio.cyclo ./Drivers/BSP/bsp_audio.d ./Drivers/BSP/bsp_audio.o ./Drivers/BSP/bsp_audio.su ./Drivers/BSP/bsp_misc.cyclo ./Drivers/BSP/bsp_misc.d ./Drivers/BSP/bsp_misc.o ./Drivers/BSP/bsp_misc.su

.PHONY: clean-Drivers-2f-BSP

