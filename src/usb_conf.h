/* USB buffer memory definition and number of string descriptors */

#ifndef __USB_CONF_H
#define __USB_CONF_H

#define HID_NUM_INTERFACES 1
#define HID_INTERFACE_0 0

#ifdef ENABLE_VIRTUAL_COM_PORT
#define VCOM_NUM_INTERFACES 2
#define VCOM_INTERFACE_0 HID_NUM_INTERFACES+0
#define VCOM_INTERFACE_1 HID_NUM_INTERFACES+1
#else
#define VCOM_NUM_INTERFACES 0
#endif
#define NUM_INTERFACES (HID_NUM_INTERFACES+VCOM_NUM_INTERFACES)

#if defined(USB_SELF_POWERED)
#define USB_INITIAL_FEATURE 0xC0   /* bmAttributes: self powered */
#else
#define USB_INITIAL_FEATURE 0x80   /* bmAttributes: bus powered */
#endif

/* Control pipe */
/* EP0: 64-byte, 64-byte  */
#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

/* HID INTR_IN */
/* EP1: 8-byte */
#define ENDP1_TXADDR        (0xc0)

/* CDC BULK_IN, INTR_IN, BULK_OUT */
/* EP3: 16-byte  */
#define ENDP3_TXADDR        (0xc8)
/* EP4: 8-byte */
#define ENDP4_TXADDR        (0xd8)
/* EP5: 16-byte */
#define ENDP5_RXADDR        (0xe0)

#endif /* __USB_CONF_H */
