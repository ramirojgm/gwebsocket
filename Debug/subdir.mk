################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../gwebsocket.c \
../gwebsocketmessage.c \
../httppackage.c \
../httprequest.c \
../httpresponse.c 

OBJS += \
./gwebsocket.o \
./gwebsocketmessage.o \
./httppackage.o \
./httprequest.o \
./httpresponse.o 

C_DEPS += \
./gwebsocket.d \
./gwebsocketmessage.d \
./httppackage.d \
./httprequest.d \
./httpresponse.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -O3 -g3 -Wall -c -fmessage-length=0 -pthread -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


