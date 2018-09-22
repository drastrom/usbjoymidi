extern uint32_t _bitband_address_out_of_range;
#define BITBAND_SRAM(x) (((uint8_t*)(x) >= (uint8_t*)0x20000000 && (uint8_t*)(x) < (uint8_t*)0x20100000) ? (volatile uint32_t*)((uint8_t*)0x22000000 + ((uint8_t*)(x) - (uint8_t*)0x20000000)*32) : (volatile uint32_t*)&_bitband_address_out_of_range[(x)])
#define BITBAND_PERIPH(x) (((uint8_t*)(x) >= (uint8_t*)0x40000000 && (uint8_t*)(x) < (uint8_t*)0x40100000) ? (volatile uint32_t*)((uint8_t*)0x42000000 + ((uint8_t*)(x) - (uint8_t*)0x40000000)*32) : (volatile uint32_t*)&_bitband_address_out_of_range[(x)])
