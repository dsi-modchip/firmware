
#ifndef DFU_H_
#define DFU_H_

#include "tusb_config.h"
#include <tusb.h>


#define DFU_ALT_COUNT 1/*6*/

#define DFU_FUNC_ATTRS (DFU_ATTR_CAN_UPLOAD | DFU_ATTR_CAN_DOWNLOAD \
		| DFU_ATTR_MANIFESTATION_TOLERANT) \

// max name length:  |||||||||||||||||||||||||||||||
#define DFU_ALT_NAMES \
					"Payload flash image DFU", \
					"Payload flash ARM7 PL1 DFU", \
					"Payload flash ARM9 PL1 DFU", \
					"Payload flash ARM7 PL2 DFU", \
					"Payload flash ARM9 PL2 DFU", \
					"Wifi board EEPROM (read-only)", \

#define DFU_ALT_RDONLY_MASK (0x20)

#define DFU_ALT_IS_RDONLY(alt) ((DFU_ALT_RDONLY_MASK & (1u << (alt))) != 0)

#endif

