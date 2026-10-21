################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Lora/Src/Lora.cpp 

OBJS += \
./Core/Lora/Src/Lora.o 

CPP_DEPS += \
./Core/Lora/Src/Lora.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Lora/Src/%.o Core/Lora/Src/%.su Core/Lora/Src/%.cyclo: ../Core/Lora/Src/%.cpp Core/Lora/Src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m33 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32U5A5xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc -I../Drivers/STM32U5xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32U5xx/Include -I../Drivers/CMSIS/Include -I"C:/projects/HITAN_reut/hitan-stm-reut/Tag/Core/Inc/platform" -I"C:/projects/HITAN_reut/hitan-stm-reut/Tag/Core/Inc/decadriver" -I"C:/projects/HITAN_reut/hitan-stm-reut/Tag/Core/Src/platform" -I"C:/projects/HITAN_reut/hitan-stm-reut/Tag/Core/Src/decadriver" -I../Drivers/BSP/STM32U5xx_Nucleo -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Lora-2f-Src

clean-Core-2f-Lora-2f-Src:
	-$(RM) ./Core/Lora/Src/Lora.cyclo ./Core/Lora/Src/Lora.d ./Core/Lora/Src/Lora.o ./Core/Lora/Src/Lora.su

.PHONY: clean-Core-2f-Lora-2f-Src

