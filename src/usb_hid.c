
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"

#include "stm32f103_local.h"
#include "usb_lld.h"
#include "usb_conf.h"
#include "usb_hid.h"

extern void put_binary (const char *, int);

#define USB_HID_REQ_GET_REPORT   1
#define USB_HID_REQ_GET_IDLE     2
#define USB_HID_REQ_GET_PROTOCOL 3
#define USB_HID_REQ_SET_REPORT   9
#define USB_HID_REQ_SET_IDLE     10
#define USB_HID_REQ_SET_PROTOCOL 11

static uint8_t hid_idle_rate;	/* in 4ms */
static union hid_report hid_report_saved;
union hid_report hid_report;

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
		return usb_lld_ctrl_recv (dev, &hid_idle_rate, 1);

	case USB_HID_REQ_GET_REPORT:
		return usb_lld_ctrl_send (dev, &hid_report, 5);

	case USB_HID_REQ_SET_REPORT:
	case USB_HID_REQ_GET_PROTOCOL:
	case USB_HID_REQ_SET_PROTOCOL:
		/* This driver doesn't support boot protocol or output. */
		return -1;

	default:
		return -1;
	}
}

void hid_write(void)
{
	// if the hid_report has changed OR it has been more than
	// hid_idle_rate * 4ms since the last report
#ifdef GNU_LINUX_EMULATION
	usb_lld_tx_enable_buf (ENDP1, &hid_report, 5);
#else
	usb_lld_write (ENDP1, &hid_report, 5);
#endif
	// TODO should have a mutex/event to wait for tx done
	hid_report_saved = hid_report;
#ifdef REALLY_VERBOSE_DEBUG
	put_binary((const char *)&hid_report, 5);
#endif
}

