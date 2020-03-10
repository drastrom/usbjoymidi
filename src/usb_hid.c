
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"

#include "stm32f103_local.h"
#include "usb_lld.h"
#include "usb_conf.h"
#include "usb_hid.h"

#ifdef DEBUG
extern void put_binary (const char *, int);
#endif

#define USB_HID_REQ_GET_REPORT   1
#define USB_HID_REQ_GET_IDLE     2
#define USB_HID_REQ_GET_PROTOCOL 3
#define USB_HID_REQ_SET_REPORT   9
#define USB_HID_REQ_SET_IDLE     10
#define USB_HID_REQ_SET_PROTOCOL 11

static chopstx_mutex_t hid_tx_mut;

static uint8_t hid_idle_rate;	/* in 4ms */
static union hid_report {
  uint64_t raw;
  struct {
    uint64_t X:15;
    uint64_t Y:15;
    uint64_t Z:15;
    uint64_t W:15;
    uint64_t button1:1;
    uint64_t button2:1;
    uint64_t button3:1;
    uint64_t button4:1;
  };
} hid_report, hid_report_saved;

void hid_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop)
{
#if !defined(GNU_LINUX_EMULATION)
	(void)dev;
#endif
	(void)interface;

	if (!stop)
#ifdef GNU_LINUX_EMULATION
		usb_lld_setup_endp (dev, ENDP1, 0, 1);
#else
		usb_lld_setup_endpoint (ENDP1, EP_INTERRUPT, 0, 0, ENDP1_TXADDR, 0);
#endif
	else
		usb_lld_stall_tx (ENDP1);
}

void hid_tx_done(uint8_t ep_num, uint16_t len)
{
	(void)ep_num;
	(void)len;
}

int hid_data_setup(struct usb_dev *dev)
{
	switch (dev->dev_req.request)
	{
	case USB_HID_REQ_GET_IDLE:
		return usb_lld_ctrl_send (dev, &hid_idle_rate, 1);
	case USB_HID_REQ_SET_IDLE:
		hid_idle_rate = (dev->dev_req.value >> 8) & 0xFF;
		return usb_lld_ctrl_ack (dev);

	case USB_HID_REQ_GET_REPORT:
		{
			int ret;
			chopstx_mutex_lock(&hid_tx_mut);
			ret = usb_lld_ctrl_send (dev, &hid_report, sizeof(hid_report));
			chopstx_mutex_unlock(&hid_tx_mut);
			return ret;
		}

	case USB_HID_REQ_SET_REPORT:
	case USB_HID_REQ_GET_PROTOCOL:
	case USB_HID_REQ_SET_PROTOCOL:
		/* This driver doesn't support boot protocol or output. */
		return -1;

	default:
		return -1;
	}
}

static void hid_write(void)
{
	// TODO: rate limit:
	// if the hid_report has changed OR it has been more than
	// hid_idle_rate * 4ms since the last report
	// Looks like both Windows and Linux set idle to 0,
	// so for now just suppress duplicate reports
	if (hid_report.raw != hid_report_saved.raw)
	{
#ifdef GNU_LINUX_EMULATION
		usb_lld_tx_enable_buf (ENDP1, &hid_report, sizeof(hid_report));
#else
		usb_lld_write (ENDP1, &hid_report, sizeof(hid_report));
#endif
		hid_report_saved = hid_report;
#if defined(DEBUG) && defined(REALLY_VERBOSE_DEBUG)
		put_binary((const char *)&hid_report, sizeof(hid_report));
#endif
	}
}

void hid_update_axes(uint16_t x, uint16_t y, uint16_t z, uint16_t w)
{
	chopstx_mutex_lock (&hid_tx_mut);
	hid_report.X = x >> 1;
	hid_report.Y = y >> 1;
	hid_report.Z = z >> 1;
	hid_report.W = w >> 1;
	hid_write();
	chopstx_mutex_unlock (&hid_tx_mut);
}

void hid_update_buttons(union hid_buttons_update update)
{
	chopstx_mutex_lock (&hid_tx_mut);
	if (update.update1)
		hid_report.button1 = update.button1;
	if (update.update2)
		hid_report.button2 = update.button2;
	if (update.update3)
		hid_report.button3 = update.button3;
	if (update.update4)
		hid_report.button4 = update.button4;
	hid_write();
	chopstx_mutex_unlock (&hid_tx_mut);
}

void hid_init(void)
{
	chopstx_mutex_init(&hid_tx_mut);
}
