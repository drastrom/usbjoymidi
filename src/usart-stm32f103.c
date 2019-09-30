/*
 * usart-stm32.c - USART driver for STM32F103 (USART2 and USART3)
 *
 * Copyright (C) 2017, 2019  g10 Code GmbH
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 * This file is a part of Chopstx, a thread library for embedded.
 *
 * Chopstx is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chopstx is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As additional permission under GNU GPL version 3 section 7, you may
 * distribute non-source form of the Program without the copy of the
 * GNU GPL normally required by section 4, provided you inform the
 * recipients of GNU GPL by a written offer.
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <chopstx.h>
#include "stm32f103_local.h"
#include "usart.h"
#include "config.h"

void put_byte_with_no_nl(uint8_t);
extern void _write (const char *s, int len);

static struct usart_stat usart3_stat;

static struct chx_intr usart3_intr;

#define BUF_A2H_SIZE 256
#define BUF_H2A_SIZE 512
static uint8_t buf_usart3_rb_a2h[BUF_A2H_SIZE];
static uint8_t buf_usart3_rb_h2a[BUF_H2A_SIZE];

static struct rb usart3_rb_a2h;
static struct rb usart3_rb_h2a;

static chopstx_poll_cond_t usart3_app_write_event;

/* Global variables so that it can be easier to debug.  */
static int usart3_tx_ready;

#define USART_DEVNO_START 3
#define USART_DEVNO_END 3

static int handle_intr (struct USART *USARTx, struct rb *rb2a, struct usart_stat *stat);
static int handle_tx (struct USART *USARTx, struct rb *rb2h, struct usart_stat *stat);
#include "usart-common.c"

int
usart_config (uint8_t dev_no, uint32_t config_bits)
{
  struct USART *USARTx = get_usart_dev (dev_no);
  uint32_t cr1_config = (USART_CR1_UE | USART_CR1_RXNEIE | USART_CR1_TE);
				/* TXEIE/TCIE will be enabled when
				   putting char */
				/* No CTSIE, PEIE, IDLEIE, LBDIE */
  (void)config_bits;
  if (USARTx == NULL)
    return -1;

  /* Disable USART before configure.  */
  USARTx->CR1 &= ~USART_CR1_UE;

  //else if ((config_bits & MASK_STOP) == STOP1B)
    USARTx->CR2 = (0x0 << 12);

  USARTx->BRR = (uint16_t)(36000000/31250);

    USARTx->CR3 = 0;

    cr1_config |= USART_CR1_RE;

  USARTx->CR1 = cr1_config;

  return 0;
}


void
usart_init0 (int (*cb) (uint8_t dev_no, uint16_t notify_bits))
{
  ss_notify_callback = cb;

  usart3_stat.dev_no = 3;
  chopstx_claim_irq (&usart3_intr, USART3_IRQ);

  /* Enable USART2 and USART3 clocks, and strobe reset.  */
  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
  RCC->APB1RSTR = RCC_APB1RSTR_USART3RST;
  RCC->APB1RSTR = 0;
}

#define UART_STATE_BITMAP_RX_CARRIER (1 << 0)
#define UART_STATE_BITMAP_TX_CARRIER (1 << 1)
#define UART_STATE_BITMAP_BREAK      (1 << 2)
#define UART_STATE_BITMAP_RINGSIGNAL (1 << 3)
#define UART_STATE_BITMAP_FRAMING    (1 << 4)
#define UART_STATE_BITMAP_PARITY     (1 << 5)
#define UART_STATE_BITMAP_OVERRUN    (1 << 6)

static int
handle_intr (struct USART *USARTx, struct rb *rb2a, struct usart_stat *stat)
{
  int tx_ready = 0;
  uint32_t r = USARTx->SR;
  int notify_bits = 0;

    {
      if ((r & USART_SR_TXE))
	{
	  tx_ready = 1;
	  USARTx->CR1 &= ~USART_CR1_TXEIE;
	}
    }

  if ((r & USART_SR_RXNE))
    {
      uint32_t data = USARTx->DR;

      /* DR register should be accessed even if data is not used.
       * Its read-access has side effect of clearing error flags.
       */
      asm volatile ("" : : "r" (data) : "memory");

      if ((r & USART_SR_NE))
	stat->err_rx_noise++;
      else if ((r & USART_SR_FE))
	{
	  /* NOTE: Noway to distinguish framing error and break  */

	  stat->rx_break++;
	  notify_bits |= UART_STATE_BITMAP_BREAK;
	}
      else if ((r & USART_SR_PE))
	{
	  stat->err_rx_parity++;
	  notify_bits |= UART_STATE_BITMAP_PARITY;
	}
      else
	{
	  if ((r & USART_SR_ORE))
	    {
	      stat->err_rx_overrun++;
	      notify_bits |= UART_STATE_BITMAP_OVERRUN;
	    }

	  /* XXX: if CS is 7-bit, mask it, or else parity bit in upper layer */
	  if (rb_ll_put (rb2a, (data & 0xff)) < 0)
	    stat->err_rx_overflow++;
	  else
	    stat->rx++;
	}
    }
  else if ((r & USART_SR_ORE))
    {				/* Clear ORE */
      uint32_t data = USARTx->DR;
      asm volatile ("" : : "r" (data) : "memory");
      stat->err_rx_overrun++;
      notify_bits |= UART_STATE_BITMAP_OVERRUN;
    }

  if (notify_bits)
    {
      if (ss_notify_callback
	  && (*ss_notify_callback) (stat->dev_no, notify_bits))
	stat->err_notify_overflow++;
    }

  return tx_ready;
}

static int
handle_tx (struct USART *USARTx, struct rb *rb2h, struct usart_stat *stat)
{
  int tx_ready = 1;
  int c = rb_ll_get (rb2h);

  if (c >= 0)
    {
      uint32_t r;

      USARTx->DR = (c & 0xff);
      stat->tx++;
      r = USARTx->SR;
	{
	  if ((r & USART_SR_TXE) == 0)
	    {
	      tx_ready = 0;
	      USARTx->CR1 |= USART_CR1_TXEIE;
	    }
	}
    }

  return tx_ready;
}

int
usart_send_break (uint8_t dev_no)
{
  struct USART *USARTx = get_usart_dev (dev_no);
  if (USARTx == NULL)
    return -1;

  if ((USARTx->CR1 & 0x01))
    return 1;	/* Busy sending break, which was requested before.  */

  USARTx->CR1 |= 0x01;
  return 0;
}

int
usart_block_sendrecv (uint8_t dev_no, const char *s_buf, uint16_t s_buflen,
		      char *r_buf, uint16_t r_buflen,
		      uint32_t *timeout_block_p, uint32_t timeout_char)
{
  uint32_t timeout;
  uint8_t *p;
  int len;
  uint32_t r;
  uint32_t data;
  struct USART *USARTx = get_usart_dev (dev_no);
  struct chx_poll_head *ph[1];

  ph[0] = (struct chx_poll_head *)&usart3_intr;

  p = (uint8_t *)s_buf;
  if (p)
    {
      USARTx->CR1 |= USART_CR1_TXEIE;

      /* Sending part */
      while (1)
	{
	  chopstx_poll (NULL, 1, ph);

	  r = USARTx->SR;

	  /* Here, ignore recv error(s).  */
	  if ((r & USART_SR_RXNE) || (r & USART_SR_ORE))
	    {
	      data = USARTx->DR;
	      asm volatile ("" : : "r" (data) : "memory");
	    }

	  if ((r & USART_SR_TXE))
	    {
	      if (s_buflen == 0)
		break;
	      else
		{
		  /* Keep TXEIE bit */
		  USARTx->DR = *p++;
		  s_buflen--;
		}
	    }

	  chopstx_intr_done (&usart3_intr);
	}

      USARTx->CR1 &= ~USART_CR1_TXEIE;

      chopstx_intr_done (&usart3_intr);
    }

  if (r_buf == NULL)
    return 0;

  /* Receiving part */
  r = chopstx_poll (timeout_block_p, 1, ph);
  if (r == 0)
    return 0;

  p = (uint8_t *)r_buf;
  len = 0;

  while (1)
    {
      r = USARTx->SR;

      data = USARTx->DR;
      asm volatile ("" : : "r" (data) : "memory");

      if ((r & USART_SR_RXNE))
	{
	  if ((r & USART_SR_NE) || (r & USART_SR_FE) || (r & USART_SR_PE))
	    /* ignore error, for now.  XXX: ss_notify */
	    ;
	  else
	    {
	      *p++ = (data & 0xff);
	      len++;
	      r_buflen--;
	      if (r_buflen == 0)
		{
		  chopstx_intr_done (&usart3_intr);
		  break;
		}
	    }
	}
      else if ((r & USART_SR_ORE))
	{
	  data = USARTx->DR;
	  asm volatile ("" : : "r" (data) : "memory");
	}

      chopstx_intr_done (&usart3_intr);
      timeout = timeout_char;
      r = chopstx_poll (&timeout, 1, ph);
      if (r == 0)
	break;
    }

  return len;
}
