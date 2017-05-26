SRCS=	blink.c \
	gpio.c \
	rcc.c \
	../hw/system_stm32f4xx.c \
	../hw/startup_stm32f4xx.s

CC=	arm-none-eabi-gcc
OBJCOPY=	arm-none-eabi-objcopy
CPPFLAGS=	-DUSE_USB_OTG_FS -I../FreeRTOS/Source/include/ -I../FreeRTOS/ -I../FreeRTOS/Source/portable/GCC/ARM_CM4F -I../hw
CFLAGS=		-Os -std=gnu99 -mthumb -mcpu=cortex-m4 -mthumb-interwork -mfloat-abi=hard -mfpu=fpv4-sp-d16 -nodefaultlibs  -nostdlib
LDFLAGS=	-Wl,--script=stm32f405_flash_bl.lds

boot-tyt.bin: boot-tyt.elf
	$(OBJCOPY) -O binary $> $@

boot-tyt.elf: $(SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

clean:
	@rm boot-tyt.elf boot-tyt.bin
