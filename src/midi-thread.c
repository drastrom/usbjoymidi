
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

union midi_event midi_event_tx;
chopstx_mutex_t midi_tx_mut;
chopstx_cond_t  midi_tx_cond;

extern void _write (const char *s, int len);
extern void put_byte_with_no_nl(uint8_t);
extern void put_int(uint32_t);
extern void put_short(uint16_t);
extern void put_binary (const char *, int);

#define STACK_PROCESS_2
#define STACK_PROCESS_7
#include "stack-def.h"
#define STACK_ADDR_USART ((uintptr_t)process2_base)
#define STACK_SIZE_USART (sizeof process2_base)
#define STACK_ADDR_MIDI ((uintptr_t)process7_base)
#define STACK_SIZE_MIDI (sizeof process7_base)

#define PRIO_MIDI 3
#define PRIO_USART 5

static void *
midi_main (void *arg)
{
	char read_byte = 0;
	(void)arg;
	chopstx_usec_wait(250*1000);
	_write("Blorg\r\n",7);
	while(usart_read(3, &read_byte, 1))
	{
		chopstx_mutex_lock(&midi_tx_mut);
		// Write single bytes (CIN = 0xF)
		// TODO parse midi and properly classify CIN
		midi_event_tx.raw = 0;
		midi_event_tx.cin = 0xF;
		midi_event_tx.cn = 0; // XXX is this the 0-based index in the endpoint descriptor's list, or is this the JackID?
		midi_event_tx.midi_0 = (uint8_t)read_byte;
		// midi_event_tx.midi_1 = midi_event_tx.midi_2 = 0;
#ifdef GNU_LINUX_EMULATION
		usb_lld_tx_enable_buf (ENDP3, &midi_event_tx, 4);
#else
		usb_lld_write (ENDP3, &midi_event_tx, 4);
#endif
		chopstx_cond_wait(&midi_tx_cond, &midi_tx_mut);
		chopstx_mutex_unlock(&midi_tx_mut);
		put_binary((const char *)&midi_event_tx, 4);
	}
	return NULL;
}

static int my_callback(uint8_t dev_no, uint16_t notify_bits)
{
	_write("callback: ", 10);
	put_int(dev_no);
	put_short(notify_bits);
	return 0;
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
