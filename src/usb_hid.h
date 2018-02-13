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

extern uint8_t hid_idle_rate;	/* in 4ms */
extern union hid_report hid_report_saved;
extern union hid_report hid_report;


