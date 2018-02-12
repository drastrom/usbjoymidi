
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"

#include "stm32f103_local.h"
#include "usb_lld.h"
#include "usb_conf.h"
#include "usb_hid.h"

extern void _write (const char *s, int len);
extern void put_byte(uint8_t);
extern void put_binary (const char *, int);

#define STACK_PROCESS_5
#include "stack-def.h"
#define STACK_ADDR_EXT ((uintptr_t)process5_base)
#define STACK_SIZE_EXT (sizeof process5_base)

#define PRIO_EXT 3

static void EXTI15_10_handler(void)
{
	uint32_t pending = EXTI->PR;
	EXTI->PR = (pending & 0xF000);
	if (pending & (1 << 12))
	{
		hid_report.st.button0 ^= 1;
	}
	if (pending & (1 << 13))
	{
		hid_report.st.button1 ^= 1;
	}
	if (pending & (1 << 14))
	{
		hid_report.st.button2 ^= 1;
	}
	if (pending & (1 << 15))
	{
		hid_report.st.button3 ^= 1;
	}
	_write("Button events: ", 15);
	put_byte((uint8_t)((pending >> 12) & 0xF));
#ifdef GNU_LINUX_EMULATION
	usb_lld_tx_enable_buf (ENDP1, &hid_report, 5);
#else
	usb_lld_write (ENDP1, &hid_report, 5);
#endif
	put_binary((const char *)&hid_report, 5);
	// TODO should have a mutex/event to wait for tx done
}

static chopstx_intr_t exti15_10_interrupt;
static struct chx_poll_head *const exti_poll[] = {
  (struct chx_poll_head *const)&exti15_10_interrupt
};

#define EXTI_POLL_NUM (sizeof (exti_poll)/sizeof (struct chx_poll_head *))

static void *
exti_main (void *arg)
{
	(void)arg;
	chopstx_usec_wait(250*1000);
	chopstx_claim_irq (&exti15_10_interrupt, EXTI15_10_IRQ);
	_write("Howdy\r\n",7);
	EXTI->PR = 0xF000;
	EXTI->IMR |= 0xF000;

	while (1)
	{
		chopstx_poll (NULL, EXTI_POLL_NUM, exti_poll);
		if (exti15_10_interrupt.ready)
		{
			EXTI15_10_handler ();
		}
	}
	return NULL;
}

void
exti_init(void)
{
	RCC->APB2RSTR = RCC_APB2RSTR_AFIORST;
	RCC->APB2RSTR = 0;
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

	/* this is the reset state */
	//GPIOB->CRH = (GPIOB->CRH & ~0xFFFF0000) | 0x44440000;

	/* EXTI12 through 15 on port B */
	AFIO->EXTICR[3] = 0x1111;
	EXTI->IMR &= ~0xF000;
	/* enable both rising and falling triggers */
	EXTI->RTSR |= 0xF000;
	EXTI->FTSR |= 0xF000;

	chopstx_create (PRIO_EXT, STACK_ADDR_EXT, STACK_SIZE_EXT, exti_main, NULL);
}
