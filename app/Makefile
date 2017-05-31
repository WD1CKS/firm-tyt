SRCS=	blink.c \
	stubs.c \
	fonts/font_8_8.c \
	../hw/controls.c \
	../hw/fault.c \
	../hw/gpio.c \
	../hw/lcd_driver.c \
	../hw/led.c \
	../hw/usb_cdc.c \
	../hw/usb/usb_bsp.c \
	../hw/usb/usb_core.c \
	../hw/usb/usb_dcd.c \
	../hw/usb/usb_dcd_int.c \
	../hw/usb/usbd_cdc_core.c \
	../hw/usb/usbd_cdc_vcp.c \
	../hw/usb/usbd_core.c \
	../hw/usb/usbd_desc.c \
	../hw/usb/usbd_ioreq.c \
	../hw/usb/usbd_req.c \
	../hw/usb/usbd_usr.c \
	../FreeRTOS/Source/list.c \
	../FreeRTOS/Source/queue.c \
	../FreeRTOS/Source/tasks.c \
	../FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c \
	../FreeRTOS/Source/portable/MemMang/heap_2.c \
	../stdperiph/system_stm32f4xx.c \
	../stdperiph/stm32f4xx_fsmc.c \
	../stdperiph/stm32f4xx_gpio.c \
	../stdperiph/stm32f4xx_rcc.c \
	../stdperiph/startup_stm32f40xx.s
	#fonts/font_8_8.c \
	#../hw/lcd_driver.c \
	#../hw/rcc.c \
	#../hw/interrupt.c \

# This is a terrible thing to do...
.ifdef GENLIST
  ASFLAGS=	-Wa,-a
  REDIR=	> firm-tyt.lst
.endif
.ifdef MD_390
  CPPFLAGS+=	-DMD_390
.endif
CC=		arm-none-eabi-gcc
AR=		arm-none-eabi-ar rcu
OBJCOPY=	arm-none-eabi-objcopy
RANLIB=		arm-none-eabi-ranlib
CPPFLAGS+=	-DUSE_USB_OTG_FS -I../FreeRTOS/Source/include/ -I../FreeRTOS/ -I../FreeRTOS/Source/portable/GCC/ARM_CM4F -Ilua/src -I../stdperiph -I../hw
CFLAGS=		-g -fverbose-asm -O2 -flto -std=gnu99 -mthumb -mcpu=cortex-m4 -mthumb-interwork -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS=	-Wl,--script=stm32f405_flash_app.lds,-Map=firm-tyt.map -L.
.export
LDLIBS=		-llua -lm
.ifdef LUA
  CFLAGS+=	-DCODEPLUGS
.endif

firm-tyt.bin: firm-tyt.img
	../md380tools/md380-fw --wrap $> $@

.PHONY: lua.a
liblua.a:
	$(MAKE) -C lua/src liblua.a
	ln -fs lua/src/liblua.a .

firm-tyt.img: firm-tyt.elf
	$(OBJCOPY) -O binary $> $@

firm-tyt.elf: $(SRCS) liblua.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ASFLAGS) $(LDFLAGS) $(SRCS) $(LDLIBS) -o $@ $(REDIR)

clean:
	$(MAKE) -C lua/src clean
	-rm firm-tyt.elf firm-tyt.bin firm-tyt.img firm-tyt.lst liblua.a
