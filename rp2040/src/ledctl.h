
#ifndef LEDCTL_H_
#define LEDCTL_H_

enum led_mode {
	ledmode_idle,
	ledmode_attack,
	ledmode_done,
	ledmode_train,
	ledmode_flash_idle,
	ledmode_flash_act,
	ledmode_error
};

void ledctl_init(void);
void ledctl_mode_set(enum led_mode mode);
void ledctl_clear_error(void);

#endif

