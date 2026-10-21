################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Lora/Inc/Sx126x_Lib/sx126x-hal.cpp \
../Core/Lora/Inc/Sx126x_Lib/sx126x.cpp 

OBJS += \
./Core/Lora/Inc/Sx126x_Lib/sx126x-hal.o \
./Core/Lora/Inc/Sx126x_Lib/sx126x.o 

CPP_DEPS += \
./Core/Lora/Inc/Sx126x_Lib/sx126x-hal.d \
./Core/Lora/Inc/Sx126x_Lib/sx126x.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Lora/Inc/Sx126x_Lib/%.o Core/Lora/Inc/Sx126x_Lib/%.su Core/Lora/Inc/Sx126x_Lib/%.cyclo: ../Core/Lora/Inc/Sx126x_Lib/%.cpp Core/Lora/Inc/Sx126x_Lib/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m33 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U5A5xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -I"C:/projects/HITAN_reut/hitan-stm-reut/Anchor/Core/Lora/Inc" -I"C:/projects/HITAN_reut/hitan-stm-reut/Anchor/Core/Inc/platform" -I"C:/projects/HITAN_reut/hitan-stm-reut/Anchor/Core/Inc/decadriver" -I"C:/projects/HITAN_reut/hitan-stm-reut/Anchor/Core/Src/platform" -I"C:/projects/HITAN_reut/hitan-stm-reut/Anchor/Core/Src/decadriver" -I../Drivers/BSP/STM32U5xx_Nucleo -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Lora-2f-Inc-2f-Sx126x_Lib

clean-Core-2f-Lora-2f-Inc-2f-Sx126x_Lib:
	-$(RM) ./Core/Lora/Inc/Sx126x_Lib/sx126x-hal.cyclo ./Core/Lora/Inc/Sx126x_Lib/sx126x-hal.d ./Core/Lora/Inc/Sx126x_Lib/sx126x-hal.o ./Core/Lora/Inc/Sx126x_Lib/sx126x-hal.su ./Core/Lora/Inc/Sx126x_Lib/sx126x.cyclo ./Core/Lora/Inc/Sx126x_Lib/sx126x.d ./Core/Lora/Inc/Sx126x_Lib/sx126x.o ./Core/Lora/Inc/Sx126x_Lib/sx126x.su

.PHONY: clean-Core-2f-Lora-2f-Inc-2f-Sx126x_Lib

