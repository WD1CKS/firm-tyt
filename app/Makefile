SRCS=	blink.c \
	../hw/usb/usbd_req.c \
	../hw/usb/usbd_core.c \
	../hw/usb/usbd_cdc_core.c \
	../hw/usb_cdc.c \
	../hw/gpio.c \
	../hw/led.c \
	../hw/usb/usbd_ioreq.c \
	../hw/usb/usbd_cdc_vcp.c \
	../hw/usb/usb_dcd_int.c \
	../hw/usb/usb_core.c \
	../hw/usb/usb_bsp.c \
	../hw/usb/usb_dcd.c \
	../hw/usb/usbd_desc.c \
	../hw/usb/usbd_usr.c \
	../hw/rcc.c \
	../hw/interrupt.c \
	../FreeRTOS/Source/tasks.c \
	../FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c \
	../FreeRTOS/Source/list.c \
	../FreeRTOS/Source/portable/MemMang/heap_2.c \
	../FreeRTOS/Source/queue.c \
	os_interface.c \
	../hw/system_stm32f4xx.c \
	../hw/startup_stm32f4xx.s

CC=	arm-none-eabi-gcc
OBJCOPY=	arm-none-eabi-objcopy
CPPFLAGS=	-DUSE_USB_OTG_FS -I../FreeRTOS/Source/include/ -I../FreeRTOS/ -I../FreeRTOS/Source/portable/GCC/ARM_CM4F -I../hw
CFLAGS=		-O2 -std=gnu99 -mthumb -mcpu=cortex-m4 -mthumb-interwork -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS=	-Wl,--script=stm32f405_flash_app.lds

firm-tyt.bin: firm-tyt.elf
	$(OBJCOPY) -O binary $> $@

firm-tyt.elf: $(SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

clean:
	rm firm-tyt.elf firm-tyt.bin
