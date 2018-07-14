union hid_report {
  uint64_t raw;
  struct {
    uint64_t X:15;
    uint64_t Y:15;
    uint64_t Z:15;
    uint64_t W:15;
    uint64_t buttons:4;
  };
};

extern union hid_report hid_report;

void hid_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop);
void hid_tx_done(uint8_t ep_num, uint16_t len);
int hid_data_setup(struct usb_dev *dev);
void hid_write(void);

