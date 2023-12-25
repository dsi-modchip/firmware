
#include <hardware/irq.h>
#include <pico/mutex.h>
#include <pico/time.h>

#include <tusb.h>

#include "thread.h"
#include "stdio_usb.h"

#include "tusb_comms.h"

struct tusb_comm tusb_comms[CFG_TUD_CDC+CFG_TUD_VENDOR];
static mutex_t tuc_mutex;
//static uint8_t low_priority_irq_num;


/*static void low_priority_worker_irq(void) {
	if (mutex_try_enter(&tuc_mutex, NULL)) {
		tud_task();
		mutex_exit(&tuc_mutex);
	}
}

static void usb_irq(void) {
	irq_set_pending(low_priority_irq_num);
}
static int64_t timer_task(__unused alarm_id_t al, __unused void *ud) {
	irq_set_pending(low_priority_irq_num);
	return PICO_STDIO_USB_TASK_INTERVAL_US;
}*/


static int tuc_read(uint8_t index, void* buf, uint32_t len) {
	int rc = PICO_ERROR_NO_DATA;

	uint32_t owner;
	if (!mutex_try_enter(&tuc_mutex, &owner)) {
		if (owner == get_core_num()) return rc; // would deadlock otherwise
		mutex_enter_blocking(&tuc_mutex);
	}

	//iprintf("[tucr] connected=%c, available=%lu\n",
	//		(tud_cdc_n_connected(index)?'T':'F'), tud_cdc_n_available(index));

#if CFG_TUD_CDC
	if (index < CFG_TUD_CDC) {
		if (tud_cdc_n_connected(index) && tud_cdc_n_available(index)) {
			uint32_t count = tud_cdc_n_read(index, buf, len);
			if (count) rc = count;
		}
	} else
#endif
#if TCFG_TUD_VENDOR
	if (index < CFG_TUD_CDC+CFG_TUD_VENDOR) {
		index -= CFG_TUD_CDC;
		if (tud_vendor_n_mounted(index) && tud_vendor_n_available(index)) {
			uint32_t count = tud_vendor_n_read(index, buf, len);
			if (count) rc = count;
		}
	} else
#endif
	{
		panic("[tucr] aaaa bad index %hhu\r\n", index);
	}

	mutex_exit(&tuc_mutex);

	//iprintf("[tucr] return %d\n", rc);
	return rc;
}

static void tuc_write(uint8_t index, const void* buf, uint32_t length) {
	static uint64_t last_avail_time[CFG_TUD_CDC+CFG_TUD_VENDOR];

	uint32_t owner;
	if (!mutex_try_enter(&tuc_mutex, &owner)) {
		if (owner == get_core_num()) return; // would deadlock otherwise
		mutex_enter_blocking(&tuc_mutex);
	}

#if CFG_TUD_CDC
	if (index < CFG_TUD_CDC) {
		uint8_t id = index - 0;

		if (tud_cdc_n_connected(id)) {
			for (uint32_t i = 0; i < length; ) {
				uint32_t n = length - i;
				uint32_t avail = tud_cdc_n_write_available(id);
				if (n > avail) n = avail;
				if (n) {
					uint32_t n2 = tud_cdc_n_write(id, buf+i, n);
					thread_do_tud_task();
					tud_cdc_n_write_flush(id);
					i += n2;
					last_avail_time[index] = time_us_64();
				} else {
					thread_do_tud_task();
					tud_cdc_n_write_flush(id);
					if (!tud_cdc_n_connected(id) ||
							(!tud_cdc_n_write_available(id) &&
							 time_us_64() > last_avail_time[index] + PICO_STDIO_USB_STDOUT_TIMEOUT_US)) {
						break;
					}
				}
			}
		} else {
			last_avail_time[index] = 0;
		}
	} else
#endif
#if CFG_TUD_VENDOR
	if (index < CFG_TUD_CDC+CFG_TUD_VENDOR) {
		uint8_t id = index - CFG_TUD_CDC;

		if (tud_vendor_n_mounted(id)) {
			for (uint32_t i = 0; i < length; ) {
				uint32_t n = length - i;
				uint32_t avail = tud_vendor_n_write_available(id);
				if (n > avail) n = avail;
				if (n) {
					uint32_t n2 = tud_vendor_n_write(id, buf+i, n);
					thread_do_tud_task();
					i += n2;
					last_avail_time[index] = time_us_64();
				} else {
					thread_do_tud_task();
					if (!tud_vendor_n_mounted(id) ||
							(!tud_vendor_n_write_available(id) &&
							 time_us_64() > last_avail_time[index] + PICO_STDIO_USB_STDOUT_TIMEOUT_US)) {
						break;
					}
				}
			}
		} else {
			last_avail_time[index] = 0;
		}
	} else
#endif
	{
		panic("[tucw] aaaa bad index %hhu\r\n", index);
	}

	mutex_exit(&tuc_mutex);
}

#if CFG_TUD_VENDOR
static bool tuc_vnd_n_conn(uint8_t index) {
	index -= CFG_TUD_CDC;
	return tud_vendor_n_mounted(index);
}
static uint32_t tuc_vnd_n_avail(uint8_t index) {
	index -= CFG_TUD_CDC;
	return tud_vendor_n_available(index);
}
#endif


void tusb_comm_init(void) {
	assert(tud_inited());

	for (size_t i = 0; i < count_of(tusb_comms); ++i) {
		tusb_comms[i].index = i;
#if CFG_TUD_CDC
		if (i < CFG_TUD_CDC) {
			tusb_comms[i].connected = tud_cdc_n_connected;
			tusb_comms[i].available = tud_cdc_n_available;
			tusb_comms[i].read = tuc_read;
			tusb_comms[i].write = tuc_write;
		} else
#endif
#if CFG_TUD_VENDOR
		if (i < CFG_TUD_CDC+CFG_TUD_VENDOR) {
			tusb_comms[i].connected = tuc_vnd_n_conn ;
			tusb_comms[i].available = tuc_vnd_n_avail;
			tusb_comms[i].read = tuc_read;
			tusb_comms[i].write = tuc_write;
		}
#endif
		{ }
	}

	mutex_init(&tuc_mutex);

	/*low_priority_irq_num = (uint8_t)user_irq_claim_unused(true);
	irq_set_exclusive_handler(low_priority_irq_num, low_priority_worker_irq);
	irq_set_enabled(low_priority_irq_num, true);

	if (irq_has_shared_handler(USBCTRL_IRQ)) {
		irq_add_shared_handler(USBCTRL_IRQ, usb_irq, PICO_SHARED_IRQ_HANDLER_LOWEST_ORDER_PRIORITY);
	} else {
		add_alarm_in_us(PICO_STDIO_USB_TASK_INTERVAL_US, timer_task, NULL, true);
	}*/
}

