/* cpu.h - NMOS 6502 CPU state for recompiled VIC-20 code.
 *
 * Recompiled routines manipulate this state directly (registers, flags, the
 * stack page) and call mem_read/mem_write for memory access. This is the
 * standard 6502 programmer's model; the VIC-20 runs the NMOS part, so the
 * decimal-mode ADC/SBC use the NMOS rules (binary N/Z reflect the wrapped
 * result) and JMP indirect honors the page-wrap bug (see mem.h). */
#ifndef VIC20RECOMP_CPU_H
#define VIC20RECOMP_CPU_H

#include <stdint.h>

typedef struct {
    uint8_t  a, x, y;   /* accumulator, index X, index Y     */
    uint8_t  s;         /* stack pointer (page 0x01)         */
    uint16_t pc;        /* program counter                   */
    /* Processor status flags, stored unpacked for cheap access. */
    uint8_t  n, v, d, i, z, c;  /* sign, overflow, decimal, irq-disable, zero, carry */
} vic_cpu_t;

extern vic_cpu_t vic_cpu;

/* Pack/unpack P (used by PHP/PLP/BRK/RTI and recompiled flag ops). */
uint8_t vic_p_pack(void);
void    vic_p_unpack(uint8_t p);

void vic_cpu_reset(void);

#endif /* VIC20RECOMP_CPU_H */
