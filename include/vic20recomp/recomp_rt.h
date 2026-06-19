/* recomp_rt.h - NMOS 6502 semantic helpers for recompiled code.
 *
 * Generated functions stay readable by calling these named helpers instead of
 * inlining flag math everywhere. Each helper updates vic_cpu (registers + N/V/
 * Z/C, and D/I where relevant) exactly as the CPU would. Memory is reached
 * through vic_mem_read/write; the emitter computes effective addresses inline
 * and passes values/targets here.
 *
 * Naming: vic_<mnemonic> for the common ops. Read-modify-write and shift ops
 * that apply to either A or memory take/return a value (vic_alu_*).
 */
#ifndef VIC20RECOMP_RECOMP_RT_H
#define VIC20RECOMP_RECOMP_RT_H

#include <stdint.h>
#include "cpu.h"
#include "mem.h"

/* --- flag helper --- */
static inline void vic_set_nz(uint8_t v) {
    vic_cpu.z = (v == 0);
    vic_cpu.n = (v >> 7) & 1;
}

/* Zero-page indirect pointer: read a 16-bit LE pointer from zero page, with
 * the 6502 zero-page wrap on the high byte (used by (zp,X) and (zp),Y). */
static inline uint16_t vic_zp_ptr(uint8_t zp) {
    return (uint16_t)(vic_mem_read(zp) | (vic_mem_read((uint8_t)(zp + 1)) << 8));
}

/* --- control-transfer hooks the emitter targets when control leaves the
 * recompiled image (ROM/monitor/DOS calls, computed jumps, undocumented or
 * not-yet-translated opcodes). Defaults live in recomp_rt.c; a host may
 * override behavior by setting these function pointers. --- */
extern void (*vic_hook_ext_call)(uint16_t addr);     /* JSR out of image        */
extern void (*vic_hook_ext_jmp)(uint16_t addr);      /* JMP out of image        */
extern void (*vic_hook_jmp_indirect)(uint16_t addr); /* JMP (ptr) computed dest */
extern void (*vic_hook_unimpl)(uint8_t opcode);      /* opcode not yet emitted  */

void vic_ext_call(uint16_t addr);
void vic_ext_jmp(uint16_t addr);
void vic_jmp_indirect(uint16_t addr);
void vic_unimpl(uint8_t opcode);

/* --- function dispatch: run recompiled C through runtime-computed targets ---
 * A static JSR/JMP abs is emitted as a direct C call, but computed jumps
 * (JMP (ptr)) and the IRQ/reset vectors only know their target at runtime. Each
 * recompiled function registers its address here; transfers to a runtime
 * address go through vic_call_addr(). vic_recomp_register() (emitted with the
 * functions) populates the table. */
typedef void (*vic_fn_t)(void);

extern uint16_t vic_last_ext_addr;                 /* last unresolved dispatch addr */
extern uint8_t  vic_last_unimpl;                   /* last unimplemented opcode     */

void vic_register(uint16_t addr, vic_fn_t fn);      /* map addr -> recompiled fn   */
void vic_call_addr(uint16_t addr);                 /* call the fn registered @addr */
int  vic_has_func(uint16_t addr);                  /* is a fn registered @addr?   */
void vic_dispatch_reset(void);                     /* clear the table             */

/* Fallback for an address with no recompiled function (a discovery gap): a host
 * can set this to an interpreter that runs the missing code, bridging
 * recompiled and interpreted execution while coverage grows. */
extern void (*vic_dispatch_fallback)(uint16_t addr);

/* --- execution model ---
 * The recompiled main loop never returns, so the runtime drives time
 * cooperatively. The emitter inserts vic_tick() at loop back-edges: it advances
 * the cycle counter and lets the host present a frame / deliver an interrupt.
 * vic_rti() pops the bytes an interrupt delivery pushed. */
#define VIC_TICK_CYCLES 8                           /* cycles charged per back-edge */

void vic_tick(void);                               /* loop back-edge: advance time */
void vic_irq_deliver(void);                        /* push state + call $FFFE handler */
extern int vic_irq_pending;                        /* a peripheral raised a maskable IRQ */
void vic_rti(void);                                /* pop the IRQ-pushed P/PC       */
uint64_t vic_cycles(void);                         /* running CPU cycle count       */

extern void (*vic_frame_hook)(void);               /* called on each vic_tick()      */

/* --- loads --- */
static inline void vic_lda(uint8_t v) { vic_cpu.a = v; vic_set_nz(v); }
static inline void vic_ldx(uint8_t v) { vic_cpu.x = v; vic_set_nz(v); }
static inline void vic_ldy(uint8_t v) { vic_cpu.y = v; vic_set_nz(v); }

/* --- logical (on A) --- */
static inline void vic_ora(uint8_t v) { vic_cpu.a |= v; vic_set_nz(vic_cpu.a); }
static inline void vic_and(uint8_t v) { vic_cpu.a &= v; vic_set_nz(vic_cpu.a); }
static inline void vic_eor(uint8_t v) { vic_cpu.a ^= v; vic_set_nz(vic_cpu.a); }

/* --- BIT --- */
static inline void vic_bit(uint8_t v) {
    vic_cpu.z = ((vic_cpu.a & v) == 0);
    vic_cpu.n = (v >> 7) & 1;
    vic_cpu.v = (v >> 6) & 1;
}

/* --- ADC/SBC (binary + NMOS decimal) --- */
static inline void vic_adc(uint8_t v) {
    unsigned c = vic_cpu.c;
    if (!vic_cpu.d) {
        unsigned sum = vic_cpu.a + v + c;
        vic_cpu.v = ((~(vic_cpu.a ^ v) & (vic_cpu.a ^ sum)) >> 7) & 1;
        vic_cpu.c = (sum > 0xFF);
        vic_cpu.a = (uint8_t)sum;
        vic_set_nz(vic_cpu.a);
    } else {
        /* NMOS decimal: N/Z/V are set from the *binary* intermediate, only the
         * result digits and carry are decimal-corrected. */
        unsigned bin = vic_cpu.a + v + c;
        unsigned lo  = (vic_cpu.a & 0x0F) + (v & 0x0F) + c;
        unsigned hi  = (vic_cpu.a >> 4)  + (v >> 4);
        vic_cpu.z = ((bin & 0xFF) == 0);
        if (lo > 9) { lo += 6; hi += 1; }
        vic_cpu.n = (hi & 0x08) ? 1 : 0;
        vic_cpu.v = ((~(vic_cpu.a ^ v) & (vic_cpu.a ^ (hi << 4))) >> 7) & 1;
        if (hi > 9) hi += 6;
        vic_cpu.c = (hi > 0x0F);
        vic_cpu.a = (uint8_t)((hi << 4) | (lo & 0x0F));
    }
}
static inline void vic_sbc(uint8_t v) {
    unsigned c = vic_cpu.c;
    unsigned diff = vic_cpu.a - v - (1 - c);
    vic_cpu.v = (((vic_cpu.a ^ v) & (vic_cpu.a ^ diff)) >> 7) & 1;
    if (!vic_cpu.d) {
        vic_cpu.c = (diff < 0x100);
        vic_cpu.a = (uint8_t)diff;
        vic_set_nz(vic_cpu.a);
    } else {
        int lo = (vic_cpu.a & 0x0F) - (v & 0x0F) - (1 - c);
        int hi = (vic_cpu.a >> 4)  - (v >> 4);
        if (lo < 0) { lo -= 6; hi -= 1; }
        if (hi < 0) hi -= 6;
        vic_cpu.c = (diff < 0x100);
        vic_set_nz((uint8_t)diff);          /* NMOS: flags from binary result */
        vic_cpu.a = (uint8_t)((hi << 4) | (lo & 0x0F));
    }
}

/* --- compares --- */
static inline void vic_cmp_reg(uint8_t reg, uint8_t v) {
    uint8_t r = (uint8_t)(reg - v);
    vic_cpu.c = (reg >= v);
    vic_set_nz(r);
}
static inline void vic_cmp(uint8_t v) { vic_cmp_reg(vic_cpu.a, v); }
static inline void vic_cpx(uint8_t v) { vic_cmp_reg(vic_cpu.x, v); }
static inline void vic_cpy(uint8_t v) { vic_cmp_reg(vic_cpu.y, v); }

/* --- inc/dec (registers) --- */
static inline void vic_inx(void) { vic_cpu.x++; vic_set_nz(vic_cpu.x); }
static inline void vic_iny(void) { vic_cpu.y++; vic_set_nz(vic_cpu.y); }
static inline void vic_dex(void) { vic_cpu.x--; vic_set_nz(vic_cpu.x); }
static inline void vic_dey(void) { vic_cpu.y--; vic_set_nz(vic_cpu.y); }

/* --- inc/dec & shifts on a value (A or memory) --- */
static inline uint8_t vic_alu_inc(uint8_t v) { v++; vic_set_nz(v); return v; }
static inline uint8_t vic_alu_dec(uint8_t v) { v--; vic_set_nz(v); return v; }
static inline uint8_t vic_alu_asl(uint8_t v) { vic_cpu.c = (v >> 7) & 1; v = (uint8_t)(v << 1); vic_set_nz(v); return v; }
static inline uint8_t vic_alu_lsr(uint8_t v) { vic_cpu.c = v & 1; v = v >> 1; vic_set_nz(v); return v; }
static inline uint8_t vic_alu_rol(uint8_t v) { unsigned nc = (v >> 7) & 1; v = (uint8_t)((v << 1) | vic_cpu.c); vic_cpu.c = nc; vic_set_nz(v); return v; }
static inline uint8_t vic_alu_ror(uint8_t v) { unsigned nc = v & 1; v = (uint8_t)((v >> 1) | (vic_cpu.c << 7)); vic_cpu.c = nc; vic_set_nz(v); return v; }

/* --- transfers --- */
static inline void vic_tax(void) { vic_cpu.x = vic_cpu.a; vic_set_nz(vic_cpu.x); }
static inline void vic_tay(void) { vic_cpu.y = vic_cpu.a; vic_set_nz(vic_cpu.y); }
static inline void vic_txa(void) { vic_cpu.a = vic_cpu.x; vic_set_nz(vic_cpu.a); }
static inline void vic_tya(void) { vic_cpu.a = vic_cpu.y; vic_set_nz(vic_cpu.a); }
static inline void vic_tsx(void) { vic_cpu.x = vic_cpu.s; vic_set_nz(vic_cpu.x); }
static inline void vic_txs(void) { vic_cpu.s = vic_cpu.x; } /* no flags */

/* --- stack --- */
static inline void    vic_push(uint8_t v) { vic_mem_write(0x0100 + vic_cpu.s, v); vic_cpu.s--; }
static inline uint8_t vic_pull(void)      { vic_cpu.s++; return vic_mem_read(0x0100 + vic_cpu.s); }
/* Push/pop a return address so recompiled JSR/RTS keep the 6502 stack pointer
 * consistent with real execution - essential for code that reads or restores SP
 * (TSX/TXS, BASIC's $F8 stack-reset). The pushed value is a real return
 * address; recompiled control flow is still C, so the popped value is unused. */
static inline void vic_push16(uint16_t a) { vic_push((uint8_t)(a >> 8)); vic_push((uint8_t)(a & 0xFF)); }
static inline void vic_pop_ret(void)      { (void)vic_pull(); (void)vic_pull(); }
/* Recompiled RTS: pop the target into the PC and return to the driver, which
 * dispatches it. This makes a normal RTS, BASIC's RTS-trick (RTS to a pushed
 * handler address), and tail dispatch all work through one mechanism - the host
 * loop ping-pongs between routines iteratively instead of via C recursion. */
static inline void vic_rts_ret(void) {
    uint16_t lo = vic_pull(); uint16_t hi = vic_pull();
    vic_cpu.pc = (uint16_t)((lo | (hi << 8)) + 1);
}
static inline void vic_pha(void) { vic_push(vic_cpu.a); }
static inline void vic_php(void) { vic_push(vic_p_pack()); }
static inline void vic_pla(void) { vic_cpu.a = vic_pull(); vic_set_nz(vic_cpu.a); }
static inline void vic_plp(void) { vic_p_unpack(vic_pull()); }

#endif /* VIC20RECOMP_RECOMP_RT_H */
