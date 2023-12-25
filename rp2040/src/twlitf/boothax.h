
#ifndef BOOTHAX_H_
#define BOOTHAX_H_

#include "mcenv.h"


enum boothax_mode {
	boothax_idle,     // ???
	boothax_safemode, // do nothing until next reset
	boothax_flash,    // do nothing (eg for USB DFU)
	boothax_attack,   // attack target
	boothax_train,    // parameter optimization
};


enum boothax_mode boothax_mode_current(void);

void boothax_init(enum mcenv_modeflags initmode);

void boothax_thread(void);

void boothax_notify_modechg(enum mcenv_modeflags newmode);
void boothax_notify_dwmavail(bool avail);
void boothax_notify_done(void);
void boothax_notify_attempt(uint32_t offset, uint32_t length);

void boothax_notify_strmsg(const char* msg);

#endif

