union midi_event {
  struct {
    uint8_t cin:4; // Code Index Number
    uint8_t cn :4; // Cable Number
    uint8_t midi_0;
    uint8_t midi_1;
    uint8_t midi_2;
  };
  uint32_t raw;
};

void midi_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop);
void midi_tx_done(uint8_t ep_num, uint16_t len);
void midi_rx_ready(uint8_t ep_num, uint16_t len);
void midi_prepare_receive(void);

