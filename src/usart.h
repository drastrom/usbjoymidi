struct usart_stat {
  uint8_t dev_no;

  uint32_t tx;
  uint32_t rx;
  uint32_t rx_break;
  uint32_t err_notify_overflow;
  uint32_t err_rx_overflow;	/* software side */
  uint32_t err_rx_overrun;	/* hardware side */
  uint32_t err_rx_noise;
  uint32_t err_rx_parity;
};


void usart_init (uint16_t prio, uintptr_t stack_addr, size_t stack_size,
		 int (*ss_notify_callback) (uint8_t dev_no, uint16_t notify_bits));
int usart_config (uint8_t dev_no, uint32_t config_bits);
int usart_read (uint8_t dev_no, char *buf, uint16_t buflen);
int usart_write (uint8_t dev_no, char *buf, uint16_t buflen);
const struct usart_stat *usart_stat (uint8_t dev_no);
int usart_send_break (uint8_t dev_no);
void usart_config_clken (uint8_t dev_no, int on);

void usart_read_prepare_poll (uint8_t dev_no, chopstx_poll_cond_t *poll_desc);
int usart_read_ext (uint8_t dev_no, char *buf, uint16_t buflen, uint32_t *timeout_p);

void usart_init0 (int (*cb) (uint8_t dev_no, uint16_t notify_bits));
int usart_block_sendrecv (uint8_t dev_no, const char *s_buf, uint16_t s_buflen,
			  char *r_buf, uint16_t r_buflen,
			  uint32_t *timeout_block_p, uint32_t timeout_char);
