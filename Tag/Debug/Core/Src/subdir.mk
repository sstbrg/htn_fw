################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/Led.cpp \
../Core/Src/Power.cpp \
../Core/Src/dw1000.cpp \
../Core/Src/flash_handler.cpp \
../Core/Src/main.cpp 

C_SRCS += \
../Core/Src/BMI088.c \
../Core/Src/adc.c \
../Core/Src/burn_mode.c \
../Core/Src/crc.c \
../Core/Src/flash.c \
../Core/Src/gpdma.c \
../Core/Src/gpio.c \
../Core/Src/i2c.c \
../Core/Src/icache.c \
../Core/Src/memorymap.c \
../Core/Src/rrc2040driver.c \
../Core/Src/rtc.c \
../Core/Src/spi.c \
../Core/Src/stm32u5xx_hal_msp.c \
../Core/Src/stm32u5xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32u5xx.c \
../Core/Src/tim.c \
../Core/Src/uart_communication.c \
../Core/Src/usart.c 

C_DEPS += \
./Core/Src/BMI088.d \
./Core/Src/adc.d \
./Core/Src/burn_mode.d \
./Core/Src/crc.d \
./Core/Src/flash.d \
./Core/Src/gpdma.d \
./Core/Src/gpio.d \
./Core/Src/i2c.d \
./Core/Src/icache.d \
./Core/Src/memorymap.d \
./Core/Src/rrc2040driver.d \
./Core/Src/rtc.d \
./Core/Src/spi.d \
./Core/Src/stm32u5xx_hal_msp.d \
./Core/Src/stm32u5xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32u5xx.d \
./Core/Src/tim.d \
./Core/Src/uart_communication.d \
./Core/Src/usart.d 

OBJS += \
./Core/Src/BMI088.o \
./Core/Src/Led.o \
./Core/Src/Power.o \
./Core/Src/adc.o \
./Core/Src/burn_mode.o \
./Core/Src/crc.o \
./Core/Src/dw1000.o \
./Core/Src/flash.o \
./Core/Src/flash_handler.o \
./Core/Src/gpdma.o \
./Core/Src/gpio.o \
./Core/Src/i2c.o \
./Core/Src/icache.o \
./Core/Src/main.o \
./Core/Src/memorymap.o \
./Core/Src/rrc2040driver.o \
./Core/Src/rtc.o \
./Core/Src/spi.o \
./Core/Src/stm32u5xx_hal_msp.o \
./Core/Src/stm32u5xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32u5xx.o \
./Core/Src/tim.o \
./Core/Src/uart_communication.o \
./Core/Src/usart.o 

CPP_DEPS += \
./Core/Src/Led.d \
./Core/Src/Power.d \
./Core/Src/dw1000.d \
./Core/Src/flash_handler.d \
./Core/Src/main.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m33 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U5A5xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -I"../Core/Inc/platform" -I"../Core/Inc/decadriver" -I"../Core/Src/platform" -I"../Core/Src/decadriver" -I../Drivers/BSP/STM32U5xx_Nucleo -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.cpp Core/Src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m33 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U5A5xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -I"../Core/Inc/platform" -I"../Core/Inc/decadriver" -I"../Core/Src/platform" -I"../Core/Src/decadriver" -I../Drivers/BSP/STM32U5xx_Nucleo -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/BMI088.cyclo ./Core/Src/BMI088.d ./Core/Src/BMI088.o ./Core/Src/BMI088.su ./Core/Src/Led.cyclo ./Core/Src/Led.d ./Core/Src/Led.o ./Core/Src/Led.su ./Core/Src/Power.cyclo ./Core/Src/Power.d ./Core/Src/Power.o ./Core/Src/Power.su ./Core/Src/adc.cyclo ./Core/Src/adc.d ./Core/Src/adc.o ./Core/Src/adc.su ./Core/Src/burn_mode.cyclo ./Core/Src/burn_mode.d ./Core/Src/burn_mode.o ./Core/Src/burn_mode.su ./Core/Src/crc.cyclo ./Core/Src/crc.d ./Core/Src/crc.o ./Core/Src/crc.su ./Core/Src/dw1000.cyclo ./Core/Src/dw1000.d ./Core/Src/dw1000.o ./Core/Src/dw1000.su ./Core/Src/flash.cyclo ./Core/Src/flash.d ./Core/Src/flash.o ./Core/Src/flash.su ./Core/Src/flash_handler.cyclo ./Core/Src/flash_handler.d ./Core/Src/flash_handler.o ./Core/Src/flash_handler.su ./Core/Src/gpdma.cyclo ./Core/Src/gpdma.d ./Core/Src/gpdma.o ./Core/Src/gpdma.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/i2c.cyclo ./Core/Src/i2c.d ./Core/Src/i2c.o ./Core/Src/i2c.su ./Core/Src/icache.cyclo ./Core/Src/icache.d ./Core/Src/icache.o ./Core/Src/icache.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/memorymap.cyclo ./Core/Src/memorymap.d ./Core/Src/memorymap.o ./Core/Src/memorymap.su ./Core/Src/rrc2040driver.cyclo ./Core/Src/rrc2040driver.d ./Core/Src/rrc2040driver.o ./Core/Src/rrc2040driver.su ./Core/Src/rtc.cyclo ./Core/Src/rtc.d ./Core/Src/rtc.o ./Core/Src/rtc.su ./Core/Src/spi.cyclo ./Core/Src/spi.d ./Core/Src/spi.o ./Core/Src/spi.su ./Core/Src/stm32u5xx_hal_msp.cyclo ./Core/Src/stm32u5xx_hal_msp.d ./Core/Src/stm32u5xx_hal_msp.o ./Core/Src/stm32u5xx_hal_msp.su ./Core/Src/stm32u5xx_it.cyclo ./Core/Src/stm32u5xx_it.d ./Core/Src/stm32u5xx_it.o ./Core/Src/stm32u5xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32u5xx.cyclo ./Core/Src/system_stm32u5xx.d ./Core/Src/system_stm32u5xx.o ./Core/Src/system_stm32u5xx.su ./Core/Src/tim.cyclo ./Core/Src/tim.d ./Core/Src/tim.o ./Core/Src/tim.su ./Core/Src/uart_communication.cyclo ./Core/Src/uart_communication.d ./Core/Src/uart_communication.o ./Core/Src/uart_communication.su ./Core/Src/usart.cyclo ./Core/Src/usart.d ./Core/Src/usart.o ./Core/Src/usart.su

.PHONY: clean-Core-2f-Src

