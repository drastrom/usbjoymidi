union hid_report {
  uint64_t raw;
  struct {
    uint16_t X;
    uint16_t Y;
    union {
      uint8_t raw;
      struct {
        uint8_t  button0:1;
        uint8_t  button1:1;
        uint8_t  button2:1;
        uint8_t  button3:1;
      };
    } buttons;
  };
};

extern union hid_report hid_report;

void hid_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop);
void hid_tx_done(uint8_t ep_num, uint16_t len);
int hid_data_setup(struct usb_dev *dev);
void hid_write(void);

