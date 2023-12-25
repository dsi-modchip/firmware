
#ifndef CSMUX_H_
#define CSMUX_H_

#include <stdint.h>
#include <stdbool.h>

enum csmux_mode {
	csmux_none,
	csmux_to_cs2,
	csmux_to_cs3
};

bool csmux_init(enum csmux_mode initmode);
void csmux_deinit(void);

void csmux_switch(enum csmux_mode mode);

#endif

