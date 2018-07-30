
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"
#include "usart.h"

#include "stm32f103_local.h"
#include "usb_lld.h"
#include "usb_conf.h"
#include "usb_midi.h"

static union midi_event midi_event_tx = {0},
	     midi_event_realtime_tx = {{0xF, 0, 0, 0, 0}};
static chopstx_mutex_t midi_tx_mut;
static chopstx_cond_t  midi_tx_cond;

extern void _write (const char *s, int len);
#ifdef DEBUG
extern void put_byte_with_no_nl(uint8_t);
extern void put_int(uint32_t);
extern void put_short(uint16_t);
extern void put_binary (const char *, int);
#endif

#define STACK_PROCESS_2
#define STACK_PROCESS_7
#include "stack-def.h"
#define STACK_ADDR_USART ((uintptr_t)process2_base)
#define STACK_SIZE_USART (sizeof process2_base)
#define STACK_ADDR_MIDI ((uintptr_t)process7_base)
#define STACK_SIZE_MIDI (sizeof process7_base)

#define PRIO_MIDI 3
#define PRIO_USART 5

static void
midi_transmit(union midi_event * tx_event)
{
#ifdef GNU_LINUX_EMULATION
	usb_lld_tx_enable_buf (ENDP3, tx_event, 4);
#else
	usb_lld_write (ENDP3, tx_event, 4);
#endif
	chopstx_cond_wait(&midi_tx_cond, &midi_tx_mut);
#ifdef DEBUG
	put_binary((const char *)tx_event, 4);
#endif
}

static void *
midi_main (void *arg)
{
	uint8_t read_byte = 0;
	struct {
		uint8_t bytes:2;
		uint8_t in_sysex:1;
		uint8_t reserved:5;
	} flags = {0};
	(void)arg;
	chopstx_usec_wait(250*1000);
	_write("Blorg\r\n",7);
	while(usart_read(3, (char *)&read_byte, 1))
	{
		chopstx_mutex_lock(&midi_tx_mut);
		if (read_byte >= 0xF8 /*&& read_byte <= 0xFF*/)
		{
			// real-time: don't touch running status,
			// transmit immediately
			midi_event_realtime_tx.midi_0 = read_byte;
			midi_transmit(&midi_event_realtime_tx);
		}
		else
		{
			if (flags.in_sysex)
			{
				if (!(read_byte & 0x80))
				{
					*(&midi_event_tx.midi_0 + flags.bytes++) = read_byte;
					if (flags.bytes == 3)
					{
						flags.bytes = 0;
						midi_event_tx.cin = 4;
						midi_transmit(&midi_event_tx);
					}
					chopstx_mutex_unlock(&midi_tx_mut);
					continue;
				}
				else
				{
					if (read_byte == 0xF7)
						*(&midi_event_tx.midi_0 + flags.bytes++) = read_byte;
					midi_event_tx.cin = 4 + flags.bytes;
					if (flags.bytes > 0)
						midi_transmit(&midi_event_tx);
					flags.in_sysex = 0;
				}
			}

			if (read_byte & 0x80)
			{
				flags.bytes = 1;
				midi_event_tx.midi_0 = read_byte;
				if (read_byte < 0xF0)
					midi_event_tx.cin = (read_byte >> 4) & 0xF;
				else if (read_byte == 0xF0)
					flags.in_sysex = 1;
				else if (read_byte == 0xF1 || read_byte == 0xF3)
					midi_event_tx.cin = 2;
				else if (read_byte == 0xF2)
					midi_event_tx.cin = 3;
				else if (read_byte == 0xF6)
				{
					midi_event_tx.cin = 5;
					midi_transmit(&midi_event_tx);
				}
			}
			else
			{
				if (flags.bytes == 2)
				{
					flags.bytes = 1;
					midi_event_tx.midi_2 = read_byte;
					midi_transmit(&midi_event_tx);
					// clear running-status here, as it wasn't possible to do so where the flowchart said (see below)
					if ((midi_event_tx.midi_0 & 0xF0) == 0xF0)
						midi_event_tx.midi_0 = 0;
				}
				else if (midi_event_tx.midi_0 != 0)
				{
					switch ((midi_event_tx.midi_0 >> 4) & 0xF)
					{
						case 0x8:
						case 0x9:
						case 0xA:
						case 0xB:
						case 0xE:
							midi_event_tx.midi_1 = read_byte;
							flags.bytes = 2;
							break;
						case 0xC:
						case 0xD:
							midi_event_tx.midi_1 = read_byte;
							midi_transmit(&midi_event_tx);
							break;
						case 0xF:
							switch (midi_event_tx.midi_0)
							{
								case 0xF2:
									midi_event_tx.midi_1 = read_byte;
									flags.bytes = 2;
									// can't clear the running status here like the flowchart says, because it is also the byte to be sent in the transmit once the 3rd byte is received.  It'll have to be cleared post-transmit
									break;
								case 0xF1:
								case 0xF3:
									midi_event_tx.midi_1 = read_byte;
									midi_transmit(&midi_event_tx);
									// fall through
								default:
									// Ignore?!?
									midi_event_tx.midi_0 = 0;
							}
							break;
						default: // should not be possible
							break;
					}
				}
			}
		}
		chopstx_mutex_unlock(&midi_tx_mut);
	}
	return NULL;
}

static int my_callback(uint8_t dev_no, uint16_t notify_bits)
{
#ifdef DEBUG
	_write("callback: ", 10);
	put_int(dev_no);
	put_short(notify_bits);
#else
	(void)dev_no;
	(void)notify_bits;
#endif
	return 0;
}

void midi_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop)
{
#if !defined(GNU_LINUX_EMULATION)
	(void)dev;
#endif
	(void)interface;

	if (!stop)
	{
#ifdef GNU_LINUX_EMULATION
		usb_lld_setup_endp (dev, ENDP2, 1, 0);
		usb_lld_setup_endp (dev, ENDP3, 0, 1);
#else
		usb_lld_setup_endpoint (ENDP2, EP_INTERRUPT, 0, ENDP2_RXADDR, 0, 8);
		usb_lld_setup_endpoint (ENDP3, EP_INTERRUPT, 0, 0, ENDP3_TXADDR, 0);
#endif
	}
	else
	{
		usb_lld_stall_rx (ENDP2);
		usb_lld_stall_tx (ENDP3);
	}
}


#if defined(GNU_LINUX_EMULATION)
static uint8_t endp2_buf[8];
#endif

void
midi_prepare_receive(void)
{
#ifdef GNU_LINUX_EMULATION
	usb_lld_rx_enable_buf (ENDP2, endp2_buf, 8);
#else
	usb_lld_rx_enable (ENDP2);
#endif
}

void
midi_rx_ready(uint8_t ep_num, uint16_t len)
{
	(void)ep_num;
	uint16_t i;
	for (i = 0; i < len/4; i++)
	{
		union midi_event midi_event_rx;
		uint8_t bytes;
#ifdef GNU_LINUX_EMULATION
		memcpy (&midi_event_rx, endp2_buf + i*4, 4);
#else
		usb_lld_rxcpy ((uint8_t*)&midi_event_rx, ep_num, i*4, 4);
#endif
		// TODO running status to reduce uart traffic
		switch(midi_event_rx.cin)
		{
			case 0x0:
			case 0x1:
			default: // shouldn't be possible due to limited range of type
				// Reserved for future expansion.
				bytes = 0;
				break;
			case 0x5:
			case 0xf:
				bytes = 1;
				break;
			case 0x2:
			case 0x6:
			case 0xc:
			case 0xd:
				bytes = 2;
				break;
			case 0x3:
			case 0x4:
			case 0x7:
			case 0x8:
			case 0x9:
			case 0xa:
			case 0xb:
			case 0xe:
				bytes = 3;
				break;
		}
		if (bytes != 0)
			usart_write(3, (char *)&midi_event_rx.midi_0, bytes);
	}
	if (i*4 < len)
	{
		// ??? fractional midi event?
		_write("dorf",4);
	}
	midi_prepare_receive();
}

void
midi_tx_done(uint8_t ep_num, uint16_t len)
{
	(void)ep_num;
	(void)len;
	chopstx_mutex_lock (&midi_tx_mut);
	chopstx_cond_signal (&midi_tx_cond);
	chopstx_mutex_unlock (&midi_tx_mut);
}

void
midi_init(void)
{
	/* B11 in input floating, B10 in alt function push/pull 2MHz (not sure why 2MHz and not 10MHz) */
	GPIOB->CRH = (GPIOB->CRH & ~0x0000FF00) | 0x00004A00;

	chopstx_mutex_init(&midi_tx_mut);
	chopstx_cond_init(&midi_tx_cond);

	usart_init(PRIO_USART, STACK_ADDR_USART, STACK_SIZE_USART, my_callback);
	chopstx_create (PRIO_MIDI, STACK_ADDR_MIDI, STACK_SIZE_MIDI, midi_main, NULL);
}
