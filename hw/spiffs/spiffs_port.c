#include "spiffs.h"
#include "spi_flash.h"

int32_t my_spiffs_read(uint32_t addr, uint32_t size, uint8_t *dst)
{
	/* Prevent bashing the good stuff */
	if (addr < 0x100000 || addr > 0xffffff || addr+size > 0xffffff)
		return -1;
	if (size > 0xffff)
		return -1;
	sFLASH_ReadBuffer(dst, addr, size);
	return SPIFFS_OK;
}

int32_t my_spiffs_write(uint32_t addr, uint32_t size, uint8_t *src)
{
	/* Prevent bashing the good stuff */
	if (addr < 0x100000 || addr > 0xffffff || addr+size > 0xffffff)
		return -1;
	if (size > 256)
		return -1;
	sFLASH_WritePage(src, addr, size);
	return SPIFFS_OK;
}

int32_t my_spiffs_erase(uint32_t addr, uint32_t size)
{
	switch(size) {
	case 0x1000:
		sFLASH_EraseSector(addr);
		return SPIFFS_OK;
	case 0x8000:
		sFLASH_Erase32KBlock(addr);
		return SPIFFS_OK;
	case 0x10000:
		sFLASH_Erase64KBlock(addr);
		return SPIFFS_OK;
	}
	return -1;
}
