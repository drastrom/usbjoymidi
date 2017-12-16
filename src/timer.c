
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"
#include "mcu/stm32.h"
#include "mcu/stm32f103.h"

extern void _write (const char *s, int len);
extern void led_blink(int);

#define STACK_PROCESS_6
#include "stack-def.h"
#define STACK_ADDR_TIM ((uintptr_t)process6_base)
#define STACK_SIZE_TIM (sizeof process6_base)

#define PRIO_TIM 4

extern void put_int (uint32_t);

void tim3_handler (void)
{
	if ((TIM3->SR & TIM_SR_UIF))
	{
		TIM3->SR &= ~TIM_SR_UIF;
		_write("Hello\r\n", 7);
		put_int(TIM4->CNT);
	}
}


void tim4_handler (void)
{
	if ((TIM4->SR & TIM_SR_UIF))
	{
		TIM4->SR &= ~TIM_SR_UIF;
		_write("Hi\r\n", 4);
		put_int(TIM4->CNT);
	}
}

static chopstx_intr_t tim3_interrupt, tim4_interrupt;
static struct chx_poll_head *const tim_poll[] = {
  (struct chx_poll_head *const)&tim3_interrupt,
  (struct chx_poll_head *const)&tim4_interrupt
};
#define TIM_POLL_NUM (sizeof (tim_poll)/sizeof (struct chx_poll_head *))



static void *
tim_main (void *arg)
{

	(void)arg;
	chopstx_usec_wait(250*1000);
	led_blink(2);
	chopstx_claim_irq (&tim3_interrupt, TIM3_IRQ);
	chopstx_claim_irq (&tim4_interrupt, TIM4_IRQ);
	TIM3->EGR = TIM_EGR_UG;
	TIM3->SR = 0;
	TIM3->DIER = TIM_DIER_UIE;
	TIM4->SR = 0;
	TIM4->DIER = TIM_DIER_UIE;
	_write("Here\r\n",6);
	TIM3->CR1 |= TIM_CR1_CEN;

#if 0
	while (1)
	{
		while (!(TIM3->SR & TIM_SR_UIF));
		_write("Heppo\r\n" ,7);
		TIM3->SR &= ~TIM_SR_UIF;
	}
#else
	while (1)
	{
		chopstx_poll (NULL, TIM_POLL_NUM, tim_poll);
		if (tim3_interrupt.ready)
		{
			tim3_handler ();
		}
		if (tim4_interrupt.ready)
		{
			tim4_handler ();
		}
	}
#endif
	return NULL;
}

void
timer_init(void)
{
	RCC->APB1RSTR = RCC_APB1RSTR_TIM3RST|RCC_APB1RSTR_TIM4RST;
	RCC->APB1RSTR = 0;
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN|RCC_APB1ENR_TIM4EN;

	TIM3->CR1 = TIM_CR1_URS | TIM_CR1_ARPE;
	TIM3->CR2 = TIM_CR2_MMS_1;
	TIM3->SMCR = 0;
	/* slow this puppy down */
	TIM3->PSC = 36000 - 1; /* 2 kHz */
	TIM3->ARR = 10000; /* 5s */
	/* Generate UEV to upload PSC and ARR */
	TIM3->EGR = TIM_EGR_UG;

	TIM4->CR1 = TIM_CR1_URS | TIM_CR1_ARPE | TIM_CR1_OPM;
	TIM4->SMCR = TIM_SMCR_TS_1 | TIM_SMCR_SMS_1|TIM_SMCR_SMS_2;
	// TODO input capture setup
	TIM4->PSC = 0; /* 72 MHz */
	TIM4->ARR = 0xFFFF; /* 0.9102083 ms */
	/* Generate UEV to upload PSC and ARR */
	TIM4->EGR = TIM_EGR_UG;

	chopstx_create (PRIO_TIM, STACK_ADDR_TIM, STACK_SIZE_TIM, tim_main, NULL);
}
