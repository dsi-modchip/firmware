
#ifndef SPICTL_H_
#define SPICTL_H_

void spictl_on_rst(void);

void spictl_init(void);
void spictl_deinit(void);
void spictl_task_init(void);
void spictl_task(void);


#define SPICTL_NOTIFY_TRIG() do { \
		glitchitf_trigger(); \
	} while (0) \

#define SPICTL_NOTIFY_SUCCESS() do { \
		glitchitf_on_success(); \
	} while (0) \

#endif

