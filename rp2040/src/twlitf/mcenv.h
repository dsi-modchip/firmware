
#ifndef MCENV_H_
#define MCENV_H_

enum mcenv_modeflags {
	mcflg_dwm = 1<<0, // DSi WiFi board present
	mcflg_twl = 1<<1, // connected to DSi mainboard
	mcflg_usb = 1<<2, // USB connected (to computer)
	mcflg_r7  = 1<<3, // DSi hinge closed
	mcflg_rst = 1<<4, // DSi reset line asserted (i.e. GND)
};

void mcenv_init(void);
void mcenv_deinit(void);

// don't track DWM if:
// * accessing it using DFU
// * forwarding the CSMUX to it
void mcenv_track_dwm(bool do_track);

enum mcenv_modeflags mcenv_get_current(void);

#define MCENV_NOTIFY_DWM(x) do { \
		boothax_notify_dwmavail(x); \
	} while (0) \

#define MCENV_NOTIFY_TWL(x) do { \
		boothax_notify_modechg(stateflags); \
	} while (0) \

#define MCENV_NOTIFY_USB(x) do { \
		boothax_notify_modechg(stateflags); \
	} while (0) \

#define MCENV_NOTIFY_R7(x)  do { \
		boothax_notify_modechg(stateflags); \
	} while (0) \

#define MCENV_NOTIFY_RST(x) do { \
		if (!(x)) { \
			/* only interested in reset deassert */ \
			glitchitf_on_twlrst(); \
			spictl_on_rst(); \
		} \
	} while (0) \

#endif

