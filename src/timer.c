
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

void timer_interrupt (void)
{
	if ((TIM3->SR & TIM_SR_UIF))
	{
		TIM3->SR &= ~TIM_SR_UIF;
		_write("Hello\r\n", 7);
	}
}


static void *
tim_main (void *arg)
{
	chopstx_intr_t interrupt;

	(void)arg;
	chopstx_usec_wait(250*1000);
	led_blink(2);
	chopstx_claim_irq (&interrupt, TIM3_IRQ);
	TIM3->EGR = TIM_EGR_UG;
	TIM3->SR = 0;
	TIM3->DIER = TIM_DIER_UIE;
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
		chopstx_intr_wait (&interrupt);
		timer_interrupt ();
	}
#endif
	return NULL;
}

void
timer_init(void)
{
	RCC->APB1RSTR = RCC_APB1RSTR_TIM3RST;
	RCC->APB1RSTR = 0;
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

	TIM3->CR1 = TIM_CR1_URS | TIM_CR1_ARPE;
	TIM3->SMCR = 0;
	/* slow this puppy down */
	TIM3->PSC = 36000 - 1; /* 2 kHz */
	TIM3->ARR = 10000; /* 5s */
	/* Generate UEV to upload PSC and ARR */
	TIM3->EGR = TIM_EGR_UG;

	chopstx_create (PRIO_TIM, STACK_ADDR_TIM, STACK_SIZE_TIM, tim_main, NULL);
}
