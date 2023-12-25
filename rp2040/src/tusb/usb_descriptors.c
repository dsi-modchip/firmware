/*
 * This file is based on a file originally part of the
 * MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2019 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <tusb.h>
#include <pico/usb_reset_interface.h>
#include <pico/unique_id.h>

#include "stdio_usb.h"
#include "info.h"
#include "dfu.h"
#include "boothax.h"


#define TUD_VENDOR_DESCRIPTOR_EX(_itfnum, _stridx, _epout, _epin, _epsize, _subclass, _protocol) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, _subclass, _protocol, _stridx,\
  /* Endpoint Out */\
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0,\
  /* Endpoint In */\
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0 \


#define TUD_RPI_RESET_DESCRIPTOR(_itfnum, _stridx) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, RESET_INTERFACE_SUBCLASS, RESET_INTERFACE_PROTOCOL, _stridx, \

#define TUD_RPI_RESET_DESC_LEN  9


#if !PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE
#error "wut"
#endif

#define USBD_MAX_POWER_MA (250)

#define USBD_CDC0_CMD_MAX_SIZE (8)
#define USBD_CDC0_IN_OUT_MAX_SIZE (CFG_TUD_CDC_RX_BUFSIZE)

#define USBD_CDC1_CMD_MAX_SIZE (8)
#define USBD_CDC1_IN_OUT_MAX_SIZE (CFG_TUD_CDC_RX_BUFSIZE)

#define USBD_VND0_IN_OUT_MAX_SIZE (CFG_TUD_VENDOR_RX_BUFSIZE)
/*#define USBD_HID0_IN_OUT_MAX_SIZE (CFG_TUD_HID_EP_BUFSIZE)*/

#define USBD_VID (0x2e8a)
#define USBD_PID (0x000a)

enum {
	STRID_LANGID = 0,
	STRID_MANUFACTURER,
	STRID_PRODUCT,
	STRID_SERIAL,
	STRID_CONFIG,
	STRID_IF_CDC0, // SPI CDC backdoor
	STRID_IF_VND0, // glitch configuration interface
	STRID_IF_RESET,
	STRID_IF_DFU__BASE
};

enum {
	ITF_NUM_TWL_CDC0_COM, // SPI CDC backdoor
	ITF_NUM_TWL_CDC0_DATA,
	ITF_NUM_TWL_RESET,

	ITF_NUM_TWL__TOTAL,
};
enum {
	//ITF_NUM_DFU_VND0, // TODO: implement this
	ITF_NUM_DFU_DFU,
	ITF_NUM_DFU_RESET,

	ITF_NUM_DFU__TOTAL,
};
enum {
	CONFIG_TOTAL_LEN_DFU
		= TUD_CONFIG_DESC_LEN
		+ TUD_RPI_RESET_DESC_LEN
		+ TUD_DFU_DESC_LEN(DFU_ALT_COUNT)
		//+ TUD_VENDOR_DESC_LEN
		,
	CONFIG_TOTAL_LEN_TWL
		= TUD_CONFIG_DESC_LEN
		+ TUD_CDC_DESC_LEN
		+ TUD_RPI_RESET_DESC_LEN
		,
};

#define EPNUM_CDC0_OUT   0x01
#define EPNUM_CDC0_IN    0x81
#define EPNUM_CDC0_NOTIF 0x82

// Note: descriptors returned from callbacks must exist long enough for transfer to complete

static const tusb_desc_device_t desc_device = {
	.bLength		 = sizeof(tusb_desc_device_t),
	.bDescriptorType = TUSB_DESC_DEVICE,
	.bcdUSB		  = 0x0200,
	.bDeviceClass	= TUSB_CLASS_MISC,
	.bDeviceSubClass = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol = MISC_PROTOCOL_IAD,
	.bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

	.idVendor  = USBD_VID,
	.idProduct = USBD_PID,
	.bcdDevice = 0x0100,

	.iManufacturer = STRID_MANUFACTURER,
	.iProduct	  = STRID_PRODUCT,
	.iSerialNumber = STRID_SERIAL,

	.bNumConfigurations = 0x01,
};

static const uint8_t desc_configuration_twl[] = {
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TWL__TOTAL, STRID_CONFIG, CONFIG_TOTAL_LEN_TWL,
		TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, USBD_MAX_POWER_MA),

	// SPI CDC backdoor
	TUD_CDC_DESCRIPTOR(ITF_NUM_TWL_CDC0_COM, STRID_IF_CDC0, EPNUM_CDC0_NOTIF,
			USBD_CDC0_CMD_MAX_SIZE, EPNUM_CDC0_OUT, EPNUM_CDC0_IN,
			USBD_CDC0_IN_OUT_MAX_SIZE),

#if PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE
	TUD_RPI_RESET_DESCRIPTOR(ITF_NUM_TWL_RESET, STRID_IF_RESET)
#endif
};
static const uint8_t desc_configuration_dfu[] = {
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_DFU__TOTAL, STRID_CONFIG, CONFIG_TOTAL_LEN_DFU,
		TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, USBD_MAX_POWER_MA),

	// DFU
	TUD_DFU_DESCRIPTOR(ITF_NUM_DFU_DFU, DFU_ALT_COUNT, STRID_IF_DFU__BASE,
			DFU_FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),

	// NOTE: don't forget to change tusb_config.h too!
	//TUD_VENDOR_DESCRIPTOR_EX(ITF_NUM_DFU_VND0, STRID_IF_VND0, EPNUM_VND0_OUT,
	//		EPNUM_VND0_IN, USBD_VND0_IN_OUT_MAX_SIZE, INFO_USB_SUBCLASS, INFO_USB_PROTOCOL),

#if PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE
	TUD_RPI_RESET_DESCRIPTOR(ITF_NUM_DFU_RESET, STRID_IF_RESET)
#endif
};

static char INFO_SERIAL[60];

const char* INFO_get_serial() {
	if (INFO_SERIAL[0] == 0) {
		pico_get_unique_board_id_string(INFO_SERIAL, sizeof(INFO_SERIAL));
	}

	return INFO_SERIAL;
}

static const char *const string_desc_arr[] = {
	[STRID_LANGID] = (const char[]){0x09,0x04},
	[STRID_MANUFACTURER] = INFO_VENDOR,
	[STRID_PRODUCT] = INFO_PRODUCT,
	[STRID_SERIAL] = INFO_SERIAL,

	// max str length: |||||||||||||||||||||||||||||||
	[STRID_CONFIG ] = "Configuration descriptor",
	[STRID_IF_CDC0] = "SPI comms CDC interface",
	[STRID_IF_VND0] = "Glitch config interface",
#if PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE
	[STRID_IF_RESET]= "RP2040 Picoboot reset itf",
#endif
	// keep this one last
	[STRID_IF_DFU__BASE] = DFU_ALT_NAMES
};

const uint8_t *tud_descriptor_device_cb(void) {
	return (const uint8_t*)&desc_device;
}

const uint8_t *tud_descriptor_configuration_cb(__unused uint8_t index) {
	return (boothax_mode_current() == boothax_flash)
		? desc_configuration_dfu : desc_configuration_twl;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid) {
	static uint16_t _desc_str[32];

	uint8_t len;
	if (index == STRID_LANGID) {
		memcpy(&_desc_str[1], string_desc_arr[STRID_LANGID], 2);
		len = 1;
		goto end;
	} else if (index == STRID_SERIAL) {
		INFO_get_serial();
	}

	if (index >= count_of(string_desc_arr)) return NULL;

	const char *str = string_desc_arr[index];
	for (len = 0; len < count_of(_desc_str) - 1 && str[len]; ++len) {
		_desc_str[1 + len] = str[len];
	}

end:
	_desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * len + 2));
	return _desc_str;
}

