union midi_event {
  uint32_t raw;
  struct {
    uint8_t cin:4; // Code Index Number
    uint8_t cn :4; // Cable Number
    uint8_t midi_0;
    uint8_t midi_1;
    uint8_t midi_2;
  };
};

extern union midi_event midi_event_tx;

