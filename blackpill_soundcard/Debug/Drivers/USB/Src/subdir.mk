################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/USB/Src/usb.c 

OBJS += \
./Drivers/USB/Src/usb.o 

C_DEPS += \
./Drivers/USB/Src/usb.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/USB/Src/%.o Drivers/USB/Src/%.su Drivers/USB/Src/%.cyclo: ../Drivers/USB/Src/%.c Drivers/USB/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Drivers/USB/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-USB-2f-Src

clean-Drivers-2f-USB-2f-Src:
	-$(RM) ./Drivers/USB/Src/usb.cyclo ./Drivers/USB/Src/usb.d ./Drivers/USB/Src/usb.o ./Drivers/USB/Src/usb.su

.PHONY: clean-Drivers-2f-USB-2f-Src

