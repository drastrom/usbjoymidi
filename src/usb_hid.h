union hid_report {
  uint64_t raw;
  struct {
    uint64_t X:15;
    uint64_t Y:15;
    uint64_t Z:15;
    uint64_t W:15;
    uint64_t buttons:4;
  };
  struct {
    uint64_t notbuttons:60;
    uint64_t button1:1;
    uint64_t button2:1;
    uint64_t button3:1;
    uint64_t button4:1;
  };
};

union hid_buttons_update {
  uint8_t raw;
  struct {
    uint8_t update1:1;
    uint8_t update2:1;
    uint8_t update3:1;
    uint8_t update4:1;
    uint8_t button1:1;
    uint8_t button2:1;
    uint8_t button3:1;
    uint8_t button4:1;
  };
  struct {
    uint8_t updates:4;
    uint8_t buttons:4;
  };
};


void hid_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop);
void hid_tx_done(uint8_t ep_num, uint16_t len);
int hid_data_setup(struct usb_dev *dev);
void hid_update_buttons(union hid_buttons_update update);
void hid_update_axes(uint16_t x, uint16_t y, uint16_t z, uint16_t w);

