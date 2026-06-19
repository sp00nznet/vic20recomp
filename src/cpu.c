/* cpu.c - NMOS 6502 state + flag pack/unpack. See cpu.h. */
#include "vic20recomp/cpu.h"
#include "vic20recomp/mem.h"

vic_cpu_t vic_cpu;

/* P bit layout: N V 1 B D I Z C. */
uint8_t vic_p_pack(void) {
    return (uint8_t)((vic_cpu.n ? 0x80 : 0) |
                     (vic_cpu.v ? 0x40 : 0) |
                     0x20 |                       /* unused, always 1 */
                     0x10 |                       /* B - set when pushed by PHP/BRK */
                     (vic_cpu.d ? 0x08 : 0) |
                     (vic_cpu.i ? 0x04 : 0) |
                     (vic_cpu.z ? 0x02 : 0) |
                     (vic_cpu.c ? 0x01 : 0));
}

void vic_p_unpack(uint8_t p) {
    vic_cpu.n = (p & 0x80) != 0;
    vic_cpu.v = (p & 0x40) != 0;
    vic_cpu.d = (p & 0x08) != 0;
    vic_cpu.i = (p & 0x04) != 0;
    vic_cpu.z = (p & 0x02) != 0;
    vic_cpu.c = (p & 0x01) != 0;
}

void vic_cpu_reset(void) {
    vic_cpu.a = vic_cpu.x = vic_cpu.y = 0;
    vic_cpu.s = 0xFF;
    vic_cpu.n = vic_cpu.v = vic_cpu.d = vic_cpu.z = vic_cpu.c = 0;
    vic_cpu.i = 1;                 /* IRQs masked at reset */
    vic_cpu.pc = vic_mem_read16(VIC_VEC_RESET);
}
