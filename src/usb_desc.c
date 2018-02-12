/*
 * usb_desc.c - USB Descriptor
 */

#include <stdint.h>
#include <string.h>

#include "config.h"

#include "sys.h"
#include "usb_lld.h"
#include "usb_conf.h"
#include "usb-cdc.h"

/* HID report descriptor.  */
#define HID_REPORT_DESC_SIZE (sizeof (hid_report_desc))

static const uint8_t hid_report_desc[] = {
  0x05, 0x01,	    /* USAGE_PAGE (Generic Desktop) */
  0x09, 0x04,	    /* USAGE (Joystick) */
  0xa1, 0x01,	    /* COLLECTION (Application) */
  /*TODO: calibrate*/
  0x15, 0x01,       /*   LOGICAL_MINIMUM (1) */
  0x26, 0xff, 0xff, /*   LOGICAL_MAXIMUM (65535) */
#if 0
  // game pad
  0x16, 0x18, 0x01, /*   LOGICAL_MINIMUM (280) */
  0x26, 0x28, 0x3c, /*   LOGICAL_MAXIMUM (15400) */
#endif
  0x05, 0x01,	    /*   USAGE_PAGE (Generic Desktop) */
  0x09, 0x01,	    /*   USAGE (Pointer) */
  0xa1, 0x00,       /*   COLLECTION (Physical) */
  0x09, 0x30,       /*     USAGE (X) */
  0x09, 0x31,       /*     USAGE (Y) */
  0x75, 0x10,       /*     REPORT_SIZE (16) */
  0x95, 0x02,       /*     REPORT_COUNT (2) */
  0x81, 0x02,       /*     INPUT (Data,Var,Abs) */
  0xC0,             /*   END_COLLECTION */
  0x05, 0x09,       /*   USAGE_PAGE (Button) */
  0x19, 0x01,       /*   USAGE_MINIMUM (Button 1) */
  0x29, 0x04,       /*   USAGE_MAXIMUM (Button 4) */
  0x15, 0x00,       /*   LOGICAL_MINIMUM (0) */
  0x25, 0x01,       /*   LOGICAL_MAXIMUM (1) */
  0x75, 0x01,       /*   REPORT_SIZE (1) */
  0x95, 0x04,       /*   REPORT_COUNT (4) */
  0x55, 0x00,       /*   UNIT_EXPONENT (0) */
  0x65, 0x00,       /*   UNIT (None) */
  0x81, 0x02,       /*   INPUT (Data,Var,Abs) */
  /* pad to byte boundary */
  0x75, 0x01,       /*   REPORT_SIZE (1) */
  0x95, 0x04,       /*   REPORT_COUNT (4) */
  0x81, 0x01,       /*   INPUT (Constant) */
  0xc0              /* END_COLLECTION */
};

/* USB Standard Device Descriptor */
#if !defined(GNU_LINUX_EMULATION)
static const
#endif
uint8_t device_desc[] = {
  18,   /* bLength */
  DEVICE_DESCRIPTOR,     /* bDescriptorType */
  0x10, 0x01,            /* bcdUSB = 1.1 */
  0x00,   /* bDeviceClass: 0 means deferred to interface */
  0x00,   /* bDeviceSubClass */
  0x00,   /* bDeviceProtocol */
  0x40,   /* bMaxPacketSize0 */
#include "usb-vid-pid-ver.c.inc"
  1, /* Index of string descriptor describing manufacturer */
  2, /* Index of string descriptor describing product */
  3, /* Index of string descriptor describing the device's serial number */
  0x01    /* bNumConfigurations */
};

#ifdef ENABLE_VIRTUAL_COM_PORT
#define VCOM_TOTAL_LENGTH (9+5+5+4+5+7+9+7+7)
#else
#define VCOM_TOTAL_LENGTH   0
#endif

#define HID_TOTAL_LENGTH (9+9+7)

#define W_TOTAL_LENGTH (9+HID_TOTAL_LENGTH+VCOM_TOTAL_LENGTH)


/* Configuation Descriptor */
static const uint8_t config_desc[] = {
  9,			   /* bLength: Configuation Descriptor size */
  CONFIG_DESCRIPTOR,	   /* bDescriptorType: Configuration */
  W_TOTAL_LENGTH, 0x00,	   /* wTotalLength:no of returned bytes */
  NUM_INTERFACES,	   /* bNumInterfaces: */
  0x01,   /* bConfigurationValue: Configuration value */
  0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
  USB_INITIAL_FEATURE,  /* bmAttributes*/
  50,	  /* MaxPower 100 mA */

  /* Interface Descriptor */
  9,			      /* bLength: Interface Descriptor size */
  INTERFACE_DESCRIPTOR,	      /* bDescriptorType: Interface */
  HID_INTERFACE_0,	 /* bInterfaceNumber: Index of Interface */
  0x00,		  /* bAlternateSetting: Alternate setting */
  0x01,		  /* bNumEndpoints: One endpoints used */
  0x03,		  /* bInterfaceClass: HID Class */
  0x00,		  /* bInterfaceSubClass: None */
  0x00,		  /* bInterfaceProtocol: None */
  0x00,		  /* iInterface: */

  /* HID Descriptor */
  9,	        /* bLength: HID Descriptor size */
  0x21,	        /* bDescriptorType: HID */
  0x10, 0x01,   /* bcdHID: HID Class Spec release number */
  0x00,	        /* bCountryCode: Hardware target country */
  0x01,         /* bNumDescriptors: Number of HID class descriptors to follow */
  0x22,         /* bDescriptorType */
  HID_REPORT_DESC_SIZE, 0, /* wItemLength: Total length of Report descriptor */

  /*Endpoint IN1 Descriptor*/
  7,                            /* bLength: Endpoint Descriptor size */
  ENDPOINT_DESCRIPTOR,		/* bDescriptorType: Endpoint */
  0x81,				/* bEndpointAddress: (IN1) */
  0x03,				/* bmAttributes: Interrupt */
  0x08, 0x00,			/* wMaxPacketSize: 8 */
  0x0A,				/* bInterval (10ms) */

#ifdef ENABLE_VIRTUAL_COM_PORT
  /* Interface Descriptor */
  9,			      /* bLength: Interface Descriptor size */
  INTERFACE_DESCRIPTOR,	      /* bDescriptorType: Interface */
  VCOM_INTERFACE_0,	 /* bInterfaceNumber: Index of Interface */
  0x00,		  /* bAlternateSetting: Alternate setting */
  0x01,		  /* bNumEndpoints: One endpoints used */
  0x02,		  /* bInterfaceClass: Communication Interface Class */
  0x02,		  /* bInterfaceSubClass: Abstract Control Model */
  0x01,		  /* bInterfaceProtocol: Common AT commands */
  0x00,		  /* iInterface: */
  /*Header Functional Descriptor*/
  5,			    /* bLength: Endpoint Descriptor size */
  0x24,			    /* bDescriptorType: CS_INTERFACE */
  0x00,			    /* bDescriptorSubtype: Header Func Desc */
  0x10, 0x01,		    /* bcdCDC: spec release number */
  /*Call Managment Functional Descriptor*/
  5,	    /* bFunctionLength */
  0x24,	    /* bDescriptorType: CS_INTERFACE */
  0x01,	    /* bDescriptorSubtype: Call Management Func Desc */
  0x03,	    /* bmCapabilities: D0+D1 */
  VCOM_INTERFACE_1,	    /* bDataInterface */
  /*ACM Functional Descriptor*/
  4,	    /* bFunctionLength */
  0x24,	    /* bDescriptorType: CS_INTERFACE */
  0x02,	    /* bDescriptorSubtype: Abstract Control Management desc */
  0x02,	    /* bmCapabilities */
  /*Union Functional Descriptor*/
  5,		 /* bFunctionLength */
  0x24,		 /* bDescriptorType: CS_INTERFACE */
  0x06,		 /* bDescriptorSubtype: Union func desc */
  VCOM_INTERFACE_0,	 /* bMasterInterface: Communication class interface */
  VCOM_INTERFACE_1,	 /* bSlaveInterface0: Data Class Interface */
  /*Endpoint 4 Descriptor*/
  7,			       /* bLength: Endpoint Descriptor size */
  ENDPOINT_DESCRIPTOR,	       /* bDescriptorType: Endpoint */
  0x84,				   /* bEndpointAddress: (IN4) */
  0x03,				   /* bmAttributes: Interrupt */
  VIRTUAL_COM_PORT_INT_SIZE, 0x00, /* wMaxPacketSize: */
  0xFF,				   /* bInterval: */

  /*Data class interface descriptor*/
  9,			       /* bLength: Endpoint Descriptor size */
  INTERFACE_DESCRIPTOR,	       /* bDescriptorType: */
  VCOM_INTERFACE_1,	 /* bInterfaceNumber: Index of Interface */
  0x00,			   /* bAlternateSetting: Alternate setting */
  0x02,			   /* bNumEndpoints: Two endpoints used */
  0x0A,			   /* bInterfaceClass: CDC */
  0x00,			   /* bInterfaceSubClass: */
  0x00,			   /* bInterfaceProtocol: */
  0x00,			   /* iInterface: */
  /*Endpoint 5 Descriptor*/
  7,			       /* bLength: Endpoint Descriptor size */
  ENDPOINT_DESCRIPTOR,	       /* bDescriptorType: Endpoint */
  0x05,				    /* bEndpointAddress: (OUT5) */
  0x02,				    /* bmAttributes: Bulk */
  VIRTUAL_COM_PORT_DATA_SIZE, 0x00, /* wMaxPacketSize: */
  0x00,			     /* bInterval: ignore for Bulk transfer */
  /*Endpoint 3 Descriptor*/
  7,			       /* bLength: Endpoint Descriptor size */
  ENDPOINT_DESCRIPTOR,	       /* bDescriptorType: Endpoint */
  0x83,				    /* bEndpointAddress: (IN3) */
  0x02,				    /* bmAttributes: Bulk */
  VIRTUAL_COM_PORT_DATA_SIZE, 0x00, /* wMaxPacketSize: */
  0x00,				    /* bInterval */
#endif
};


/* USB String Descriptors */
static const uint8_t usbmidijoy_string_lang_id[] = {
  4,				/* bLength */
  STRING_DESCRIPTOR,
  0x09, 0x04			/* LangID = 0x0409: US-English */
};

#define USB_STRINGS 1
#include "usb-strings.c.inc"

struct desc
{
  const uint8_t *desc;
  uint16_t size;
};

static const struct desc string_descriptors[] = {
  {usbmidijoy_string_lang_id, sizeof (usbmidijoy_string_lang_id)},
  {usbmidijoy_string_vendor, sizeof (usbmidijoy_string_vendor)},
  {usbmidijoy_string_product, sizeof (usbmidijoy_string_product)},
  {usbmidijoy_string_serial, sizeof (usbmidijoy_string_serial)},
  {usbmidijoy_revision_detail, sizeof (usbmidijoy_revision_detail)},
  {usbmidijoy_config_options, sizeof (usbmidijoy_config_options)},
  {sys_version, sizeof (sys_version)},
};
#define NUM_STRING_DESC (sizeof (string_descriptors) / sizeof (struct desc))

#define USB_DT_HID			0x21
#define USB_DT_REPORT			0x22
#define USB_DT_PHYSICAL			0x23

int
usb_get_descriptor (struct usb_dev *dev)
{
  struct device_req *arg = &dev->dev_req;
  uint8_t rcp = arg->type & RECIPIENT;
  uint8_t desc_type = (arg->value >> 8);
  uint8_t desc_index = (arg->value & 0xff);

  if (rcp == DEVICE_RECIPIENT)
    {
      if (desc_type == DEVICE_DESCRIPTOR)
	return usb_lld_ctrl_send (dev, device_desc, sizeof (device_desc));
      else if (desc_type == CONFIG_DESCRIPTOR)
	return usb_lld_ctrl_send (dev, config_desc, sizeof (config_desc));
      else if (desc_type == STRING_DESCRIPTOR)
	{
	  if (desc_index < NUM_STRING_DESC)
	    return usb_lld_ctrl_send (dev, string_descriptors[desc_index].desc,
				      string_descriptors[desc_index].size);
#ifdef USE_SYS3
	  else if (desc_index == NUM_STRING_DESC)
	    {
	      uint8_t usbbuf[64];
	      int i;
	      size_t len;

	      for (i = 0; i < (int)sizeof (usbbuf)/2 - 2; i++)
		{
		  if (sys_board_name[i] == 0)
		    break;

		  usbbuf[i*2+2] = sys_board_name[i];
		  usbbuf[i*2+3] = 0;
		}
	      usbbuf[0] = len = i*2 + 2;
	      usbbuf[1] = STRING_DESCRIPTOR;
	      return usb_lld_ctrl_send (dev, usbbuf, len);
	    }
#endif
	}
    }
  else if (rcp == INTERFACE_RECIPIENT)
    {
      if (arg->index == HID_INTERFACE_0)
        {
          /* arg->index is the interface number, desc_type is descriptor
           * type, desc_index is descriptor index (0 except for physical
           * descriptors
           */
	  if (desc_type == USB_DT_HID)
	    return usb_lld_ctrl_send (dev, config_desc+9+9, 9);
	  else if (desc_type == USB_DT_REPORT)
	    return usb_lld_ctrl_send (dev, hid_report_desc,
				      HID_REPORT_DESC_SIZE);
        }
    }

  return -1;
}
