/* USB: SET_CONTROL_LINE_STATE
   DTR RTS */
/* USB: SERIAL_STATE
   DSR DCD RI */

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
