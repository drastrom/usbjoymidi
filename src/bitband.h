extern uint32_t _bitband_address_out_of_range;
#define BITBAND_SRAM(x) (((void*)(x) >= (void*)0x20000000 && (void*)(x) < (void*)0x20100000) ? (volatile uint32_t*)((void*)0x22000000 + ((void*)(x) - (void*)0x20000000)*32) : (volatile uint32_t*)&_bitband_address_out_of_range[(x)])
#define BITBAND_PERIPH(x) (((void*)(x) >= (void*)0x40000000 && (void*)(x) < (void*)0x40100000) ? (volatile uint32_t*)((void*)0x42000000 + ((void*)(x) - (void*)0x40000000)*32) : (volatile uint32_t*)&_bitband_address_out_of_range[(x)])
