/* recomp_rt.c - dispatch table, control-transfer hooks, and the cooperative
 * time/interrupt model for recompiled code. The arithmetic/flag helpers are
 * inline in recomp_rt.h; this file holds the parts with state. */
#include "vic20recomp/recomp_rt.h"
#include <string.h>

/* Last external-control event, for tests/diagnostics. */
uint16_t vic_last_ext_addr = 0;
uint8_t  vic_last_unimpl   = 0;

/* --- function dispatch table (addr -> recompiled fn) --- */
static vic_fn_t g_dispatch[0x10000];

void (*vic_dispatch_fallback)(uint16_t) = 0;

void vic_dispatch_reset(void)               { memset(g_dispatch, 0, sizeof(g_dispatch)); }
void vic_register(uint16_t addr, vic_fn_t fn){ g_dispatch[addr] = fn; }
int  vic_has_func(uint16_t addr)            { return g_dispatch[addr] != 0; }
void vic_call_addr(uint16_t addr) {
    if (g_dispatch[addr]) g_dispatch[addr]();                       /* recompiled fn */
    else if (vic_dispatch_fallback) vic_dispatch_fallback(addr);      /* interpret gap */
    else vic_last_ext_addr = addr;                                   /* record + no-op */
}

static void default_ext_call(uint16_t addr)     { vic_call_addr(addr); }
static void default_ext_jmp(uint16_t addr)      { vic_call_addr(addr); }
static void default_jmp_indirect(uint16_t addr) { vic_call_addr(addr); }
static void default_unimpl(uint8_t opcode)      { vic_last_unimpl = opcode; }

void (*vic_hook_ext_call)(uint16_t)     = default_ext_call;
void (*vic_hook_ext_jmp)(uint16_t)      = default_ext_jmp;
void (*vic_hook_jmp_indirect)(uint16_t) = default_jmp_indirect;
void (*vic_hook_unimpl)(uint8_t)        = default_unimpl;

void vic_ext_call(uint16_t addr)     { vic_hook_ext_call(addr); }
void vic_ext_jmp(uint16_t addr)      { vic_hook_ext_jmp(addr); }
void vic_jmp_indirect(uint16_t addr) { vic_hook_jmp_indirect(addr); }
void vic_unimpl(uint8_t opcode)      { vic_hook_unimpl(opcode); }

/* --- cooperative time + interrupts --- */
static uint64_t g_cycles = 0;
void (*vic_frame_hook)(void) = 0;
int vic_irq_pending = 0;

uint64_t vic_cycles(void) { return g_cycles; }

void vic_tick(void) {
    g_cycles += VIC_TICK_CYCLES;
    if (vic_frame_hook) vic_frame_hook();
    /* Deliver a maskable IRQ only when a peripheral has actually raised one.
     * The bare VIC-20 has no IRQ source, so vic_irq_pending stays 0 and loop
     * back-edges never spuriously interrupt (which would corrupt recompiled
     * code running with IRQs unmasked). A timer-driven system sets the flag. */
    if (vic_irq_pending && !vic_cpu.i) vic_irq_deliver();
}

void vic_irq_deliver(void) {
    /* Push PC + P like the CPU's IRQ sequence, then run the $FFFE handler. The
     * handler's RTI is emitted as vic_rti(), which pops these back. */
    vic_push((uint8_t)(vic_cpu.pc >> 8));
    vic_push((uint8_t)(vic_cpu.pc & 0xFF));
    vic_push((uint8_t)(vic_p_pack() & ~0x10));   /* B clear for a hardware IRQ */
    vic_cpu.i = 1;
    uint16_t vec = vic_mem_read16(VIC_VEC_IRQ);
    if (vic_has_func(vec)) vic_call_addr(vec);
}

void vic_rti(void) {
    vic_p_unpack(vic_pull());
    uint16_t lo = vic_pull();
    uint16_t hi = vic_pull();
    vic_cpu.pc = (uint16_t)(lo | (hi << 8));
}
