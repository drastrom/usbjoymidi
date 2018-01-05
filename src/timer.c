
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"
#include "usart.h"

#include "stm32f103_local.h"

extern void _write (const char *s, int len);
extern void put_int (uint32_t);
extern void led_blink(int);

#define STACK_PROCESS_6
#include "stack-def.h"
#define STACK_ADDR_TIM ((uintptr_t)process6_base)
#define STACK_SIZE_TIM (sizeof process6_base)

#define PRIO_TIM 4

static struct timer_capture
{
	volatile uint16_t capture1;
	volatile uint16_t capture2;
	volatile uint16_t capture3;
	volatile uint16_t capture4;
} timer4_capture = {0, 0, 0, 0};

/* Conversions from timer capture values to ohms and back (in python):
 *
 * import math
 * ticks_to_ohms = lambda ticks: ticks/(-0.36*math.log1p(-1.6/5))
 * ohms_to_ticks = lambda ohms: -0.36*math.log1p(-1.6/5)*ohms
 *
 * Note the 0.36 comes from 36MHz, the rate of the timer, so needs to be
 * adjusted if the prescaler value is altered (or the RCC clocks, but that
 * would require more adjustment than just here).
 *
 * Also note that the circuit includes a 2.2kOhm resistor in series with the
 * axis, so you may have to compensate for that, plus everything is an
 * approximation (not using high-precision components or anything) so it's a
 * more of a ballpark anyway.
 */

static int seen = 0;

static void DMA1_Channel7_handler(void)
{
	if (DMA1->ISR & DMA_ISR_TCIF7)
	{
		DMA1->IFCR = DMA_ISR_TCIF7;
		_write("Ho\r\n", 4);
		put_int(timer4_capture.capture1);
		put_int(timer4_capture.capture2);
		put_int(timer4_capture.capture3);
		put_int(timer4_capture.capture4);
		if(!seen++)
			usart_write(3, "\x90", 1);
		usart_write(3, "\x30\x40", 2);
	}
}

static chopstx_intr_t dma1_channel7_interrupt;
static struct chx_poll_head *const tim_poll[] = {
  (struct chx_poll_head *const)&dma1_channel7_interrupt
};
#define TIM_POLL_NUM (sizeof (tim_poll)/sizeof (struct chx_poll_head *))


static void *
tim_main (void *arg)
{

	(void)arg;
	chopstx_usec_wait(250*1000);
	led_blink(2);
	chopstx_claim_irq (&dma1_channel7_interrupt, DMA1_CHANNEL7_IRQ);
	TIM3->SR = 0;
	TIM3->DIER = TIM_DIER_UDE|TIM_DIER_CC1DE;
	TIM4->SR = 0;
	TIM4->DIER = TIM_DIER_UDE;
	DMA1->IFCR = 0x0fffffff;
	DMA1_Channel3->CCR |= DMA_CCR1_EN;
	DMA1_Channel6->CCR |= DMA_CCR1_EN;
	DMA1_Channel7->CCR |= DMA_CCR1_EN;
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
		if (dma1_channel7_interrupt.ready)
		{
			DMA1_Channel7_handler ();
		}
	}
#endif
	return NULL;
}

static uint32_t gpio_start_val = (0xF << 6), gpio_reset_val = (0xF << (6+16));

void
timer_init(void)
{
	RCC->APB1RSTR = RCC_APB1RSTR_TIM3RST|RCC_APB1RSTR_TIM4RST;
	RCC->APB1RSTR = 0;
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN|RCC_APB1ENR_TIM4EN;

	RCC->APB2RSTR = RCC_APB2RSTR_IOPBRST;
	RCC->APB2RSTR = 0;
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

	RCC->AHBENR |= RCC_AHBENR_DMA1EN;
	// TIM3_UP
	DMA1_Channel3->CCR = DMA_CCR1_DIR | DMA_CCR1_CIRC | DMA_CCR1_PSIZE_1 | DMA_CCR1_MSIZE_1 | DMA_CCR1_PL;
	DMA1_Channel3->CNDTR = 1;
	DMA1_Channel3->CPAR = (uint32_t)&GPIOB->BSRR;
	DMA1_Channel3->CMAR = (uint32_t)&gpio_start_val;
	// TIM3_CH1
	DMA1_Channel6->CCR = DMA_CCR1_DIR | DMA_CCR1_CIRC | DMA_CCR1_PSIZE_1 | DMA_CCR1_MSIZE_1 | DMA_CCR1_PL;
	DMA1_Channel6->CNDTR = 1;
	DMA1_Channel6->CPAR = (uint32_t)&GPIOB->BSRR;
	DMA1_Channel6->CMAR = (uint32_t)&gpio_reset_val;
	// TIM4_UP
	DMA1_Channel7->CCR = DMA_CCR1_TCIE | DMA_CCR1_CIRC | DMA_CCR1_MINC | DMA_CCR1_PSIZE_0 | DMA_CCR1_MSIZE_0 | DMA_CCR1_PL_0;
	DMA1_Channel7->CNDTR = 4;
	DMA1_Channel7->CPAR = (uint32_t)&TIM4->DMAR;
	DMA1_Channel7->CMAR = (uint32_t)&timer4_capture.capture1;
	chopstx_usec_wait(1);

	GPIOB->BSRR = gpio_reset_val;
	/* The docs are unclear here.  In fact they say something that doesn't seem to make sense (emulate AFI by putting in AFO) */
	/* putting in ouput open drain */
	GPIOB->CRH = (GPIOB->CRH & ~0xFF) | 0x55;
	GPIOB->CRL = (GPIOB->CRL & ~0xFF000000) | 0x55000000;

	TIM3->CR1 = TIM_CR1_URS | TIM_CR1_ARPE;
	TIM3->CR2 = TIM_CR2_MMS_1;
	TIM3->SMCR = 0;
	/* slow this puppy down */
	TIM3->PSC = 36000 - 1; /* 2 kHz */
	TIM3->ARR = 10000; /* 5s */
	/* set up output compare to reset gpio */
	TIM3->CCMR1 = 0;
	TIM3->CCR1 = 5; /* 2.5ms */
	TIM3->CCER = TIM_CCER_CC1E;
	/* Generate UEV to upload PSC and ARR */
	TIM3->EGR = TIM_EGR_UG;

	TIM4->CR1 = TIM_CR1_URS | TIM_CR1_ARPE | TIM_CR1_OPM;
	TIM4->SMCR = TIM_SMCR_TS_1 | TIM_SMCR_SMS_1|TIM_SMCR_SMS_2;
	TIM4->CCMR1 = TIM_CCMR1_CC2S_0|TIM_CCMR1_CC1S_0;
	TIM4->CCMR2 = TIM_CCMR2_CC4S_0|TIM_CCMR2_CC3S_0;
	TIM4->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
	TIM4->DCR = ((4-1) << 8) | 0xD;
	TIM4->PSC = 1; /* 36 MHz */
	TIM4->ARR = 0xFFFF; /* 1.8204166 ms */
	/* Generate UEV to upload PSC and ARR */
	TIM4->EGR = TIM_EGR_UG;

	chopstx_create (PRIO_TIM, STACK_ADDR_TIM, STACK_SIZE_TIM, tim_main, NULL);
}
