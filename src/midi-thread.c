
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"
#include "usart.h"

#include "stm32f103_local.h"

extern void _write (const char *s, int len);
extern void put_byte_with_no_nl(uint8_t);
extern void put_int(uint32_t);
extern void put_short(uint16_t);

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
	int i = 0;
	(void)arg;
	chopstx_usec_wait(250*1000);
	_write("Blorg\r\n",7);
	while(usart_read(3, &read_byte, 1))
	{
		put_byte_with_no_nl(read_byte);
		if ((i++ & 0xf) == 0xf)
			_write ("\r\n", 2);
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
midi_init(void)
{
	/* B11 in input floating, B10 in alt function push/pull 2MHz (not sure why 2MHz and not 10MHz) */
	GPIOB->CRH = (GPIOB->CRH & ~0x0000FF00) | 0x00004A00;

	usart_init(PRIO_USART, STACK_ADDR_USART, STACK_SIZE_USART, my_callback);
	chopstx_create (PRIO_MIDI, STACK_ADDR_MIDI, STACK_SIZE_MIDI, midi_main, NULL);
}
