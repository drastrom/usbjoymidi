/*
 * usart-stm32.c - USART driver for STM32F103 (USART2 and USART3)
 *
 * Copyright (C) 2017  g10 Code GmbH
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
 * receipents of GNU GPL by a written offer.
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

static struct USART *
get_usart_dev (uint8_t dev_no)
{
  if (dev_no == 2)
    return USART2;
  else if (dev_no == 3)
    return USART3;

  return NULL;
}

static void *usart_main (void *arg);

static struct usart_stat usart3_stat;

int
usart_config (uint8_t dev_no, uint32_t config_bits)
{
  struct USART *USARTx = get_usart_dev (dev_no);
  uint32_t cr1_config = (USART_CR1_UE | USART_CR1_RXNEIE
			 | USART_CR1_TE | USART_CR1_RE);
				/* TXEIE will be enabled when putting char */
				/* No CTSIE, PEIE, TCIE, IDLEIE, LBDIE */
  (void)config_bits;
  if (USARTx == NULL)
    return -1;

  /* Disable USART before configure.  */
  USARTx->CR1 &= ~USART_CR1_UE;

  //else if ((config_bits & MASK_STOP) == STOP1B)
    USARTx->CR2 = (0x0 << 12);

  USARTx->BRR = (uint16_t)(36000000/31250);
  USARTx->CR3 = 0;
  USARTx->CR1 = cr1_config;
  return 0;
}

static int (*ss_notify_callback) (uint8_t dev_no, uint16_t notify_bits);

void
usart_init (uint16_t prio, uintptr_t stack_addr, size_t stack_size,
	    int (*cb) (uint8_t dev_no, uint16_t notify_bits))
{
  ss_notify_callback = cb;
  usart3_stat.dev_no = 3;

  /* Enable USART3 clock, and strobe reset.  */
  RCC->APB1RSTR = RCC_APB1RSTR_USART3RST;
  RCC->APB1RSTR = 0;
  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

  usart_config (3, 0);
  chopstx_create (prio, stack_addr, stack_size, usart_main, NULL);
}

/*
 * Ring buffer
 */
#define MAX_RB_BUF 1024

struct rb {
  uint8_t *buf;
  chopstx_mutex_t m;
  chopstx_cond_t data_available;
  chopstx_cond_t space_available;
  uint32_t head  :10;
  uint32_t tail  :10;
  uint32_t size  :10;
  uint32_t full  : 1;
  uint32_t empty : 1;
};

/*
 * Note: size = 1024 can still work, regardless of the limit of 10-bit.
 */
static void
rb_init (struct rb *rb, uint8_t *p, uint16_t size)
{
  rb->buf = p;
  rb->size = size;
  chopstx_mutex_init (&rb->m);
  chopstx_cond_init (&rb->data_available);
  chopstx_cond_init (&rb->space_available);
  rb->head = rb->tail = 0;
  rb->full = 0;
  rb->empty = 1;
}

static void
rb_add (struct rb *rb, uint8_t v)
{
  rb->buf[rb->tail++] = v;
  if (rb->tail == rb->size)
    rb->tail = 0;
  if (rb->tail == rb->head)
    rb->full = 1;
  rb->empty = 0;
}

static uint8_t
rb_del (struct rb *rb)
{
  uint32_t v = rb->buf[rb->head++];

  if (rb->head == rb->size)
    rb->head = 0;
  if (rb->head == rb->tail)
    rb->empty = 1;
  rb->full = 0;

  return v;
}

/*
 * Application: consumer
 * Hardware:    generator
 */
static int
rb_ll_put (struct rb *rb, uint8_t v)
{
  int r;

  chopstx_mutex_lock (&rb->m);
  if (rb->full)
    r = -1;
  else
    {
      r = 0;
      rb_add (rb, v);
      chopstx_cond_signal (&rb->data_available);
    }
  chopstx_mutex_unlock (&rb->m);
  return r;
}

/*
 * Application: generator
 * Hardware:    consumer
 */
static int
rb_ll_get (struct rb *rb)
{
  int r;

  chopstx_mutex_lock (&rb->m);
  if (rb->empty)
    r = -1;
  else
    {
      r = rb_del (rb);
      chopstx_cond_signal (&rb->space_available);
    }
  chopstx_mutex_unlock (&rb->m);
  return r;
}

static void
rb_ll_flush (struct rb *rb)
{
  chopstx_mutex_lock (&rb->m);
  while (!rb->empty)
    rb_del (rb);
  chopstx_cond_signal (&rb->space_available);
  chopstx_mutex_unlock (&rb->m);
}

/*
 * Application: consumer
 * Hardware:    generator
 */
static int
rb_read (struct rb *rb, uint8_t *buf, uint16_t buflen)
{
  int i = 0;

  chopstx_mutex_lock (&rb->m);
  while (rb->empty)
    chopstx_cond_wait (&rb->data_available, &rb->m);

  while (i < buflen)
    {
      buf[i++] = rb_del (rb);
      if (rb->empty)
	break;
    }
  chopstx_cond_signal (&rb->space_available);
  chopstx_mutex_unlock (&rb->m);

  return i;
}

/*
 * Application: generator
 * Hardware:    consumer
 */
static void
rb_write (struct rb *rb, uint8_t *buf, uint16_t buflen)
{
  int i = 0;

  chopstx_mutex_lock (&rb->m);

  do
    {
      while (rb->full)
	chopstx_cond_wait (&rb->space_available, &rb->m);

      while (i < buflen)
	{
	  rb_add (rb, buf[i++]);
	  if (rb->full)
	    {
	      chopstx_cond_signal (&rb->data_available);
	      break;
	    }
	}
    }
  while (i < buflen);

  if (i)
    chopstx_cond_signal (&rb->data_available);
  chopstx_mutex_unlock (&rb->m);
}

static int
rb_empty_check (void *arg)
{
  struct rb *rb = arg;
  return rb->empty == 0;
}

static void
rb_get_prepare_poll (struct rb *rb, chopstx_poll_cond_t *poll_desc)
{
  poll_desc->type  = CHOPSTX_POLL_COND;
  poll_desc->ready = 0;
  poll_desc->cond  = &rb->data_available;
  poll_desc->mutex = &rb->m;
  poll_desc->check = rb_empty_check;
  poll_desc->arg   = rb;
}

static uint8_t buf_usart3_rb_a2h[256];
static uint8_t buf_usart3_rb_h2a[512];

static struct chx_intr usart3_intr;

static struct rb usart3_rb_a2h;
static struct rb usart3_rb_h2a;

static chopstx_poll_cond_t usart3_app_write_event;

static struct chx_poll_head *usart_poll[2];

/* Global variables so that it can be easier to debug.  */
static int usart3_tx_ready;

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

  if ((r & USART_SR_TXE))
    {
      tx_ready = 1;
      USARTx->CR1 &= ~USART_CR1_TXEIE;
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
      if ((*ss_notify_callback) (stat->dev_no, notify_bits))
	stat->err_notify_overflow++;
    }

  return tx_ready;
}

static int
handle_tx_ready (struct USART *USARTx, struct rb *rb2h,
		 struct usart_stat *stat)
{
#ifdef DEBUG
  static int n = 0;
#endif
  int tx_ready = 1;
  int c = rb_ll_get (rb2h);

  if (c >= 0)
    {
      uint32_t r;

#ifdef DEBUG
      put_byte_with_no_nl((c & 0xff));
      if (((++n) & 0xf) == 0)
	      _write("\r\n", 2);
#endif
      USARTx->DR = (c & 0xff);
      stat->tx++;
      r = USARTx->SR;
      if ((r & USART_SR_TXE) == 0)
	{
	  tx_ready = 0;
	  USARTx->CR1 |= USART_CR1_TXEIE;
	}
    }

  return tx_ready;
}

static void *
usart_main (void *arg)
{
  (void)arg;

  usart3_tx_ready = 1;

  chopstx_claim_irq (&usart3_intr, USART3_IRQ);

  rb_init (&usart3_rb_a2h, buf_usart3_rb_a2h, sizeof buf_usart3_rb_a2h);
  rb_init (&usart3_rb_h2a, buf_usart3_rb_h2a, sizeof buf_usart3_rb_h2a);

  rb_get_prepare_poll (&usart3_rb_a2h, &usart3_app_write_event);

  while (1)
    {
      int n = 0;

      usart_poll[n++] = (struct chx_poll_head *)&usart3_intr;
      if (usart3_tx_ready)
	usart_poll[n++] = (struct chx_poll_head *)&usart3_app_write_event;
      else
	usart3_app_write_event.ready = 0;

      chopstx_poll (NULL, n, usart_poll);

      if (usart3_intr.ready)
      {
	usart3_tx_ready = handle_intr (USART3, &usart3_rb_h2a, &usart3_stat);
	chopstx_intr_done(&usart3_intr);
      }

      if (usart3_tx_ready && usart3_app_write_event.ready)
	usart3_tx_ready = handle_tx_ready (USART3,
					   &usart3_rb_a2h, &usart3_stat);
    }

  return NULL;
}

int
usart_read (uint8_t dev_no, char *buf, uint16_t buflen)
{
  struct rb *rb;

  if (dev_no == 3)
    rb = &usart3_rb_h2a;
  else
    return -1;

  if (buf == NULL && buflen == 0)
    {
      rb_ll_flush (rb);
      return 0;
    }
  else
    return rb_read (rb, (uint8_t *)buf, buflen);
}

int
usart_write (uint8_t dev_no, char *buf, uint16_t buflen)
{
  struct rb *rb;

  if (dev_no == 3)
    rb = &usart3_rb_a2h;
  else
    return -1;

  if (buf == NULL && buflen == 0)
    rb_ll_flush (rb);
  else
    rb_write (rb, (uint8_t *)buf, buflen);
  return 0;
}

const struct usart_stat *
usart_stat (uint8_t dev_no)
{
  if (dev_no == 3)
    return &usart3_stat;
  else
    return NULL;
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
