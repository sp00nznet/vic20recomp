/* analyze.h - control-flow analysis over decoded 6502 code.
 *
 * A linear sweep powers the disassembler; recursive-descent function discovery
 * carves a loaded code image into functions the emitter turns into C. Seed from
 * known entry points (a binary file's load address, the reset/IRQ vectors, hint
 * addresses), follow calls/branches/jumps, and stop at the edges of the image.
 * The hard part on a 6502 is that code and data interleave and computed jumps
 * (JMP (abs), RTS-trampolines) hide targets - so discovery is seeded and, for a
 * post-load image, follows the one pointer an indirect jump reads.
 */
#ifndef M6502_ANALYZE_H
#define M6502_ANALYZE_H

#include <stdint.h>
#include <stddef.h>
#include "decode.h"

/* Called once per decoded instruction during a sweep. */
typedef void (*insn_cb)(const insn_t *in, void *user);

/* Linear sweep: decode from rom[start_off] for `count` instructions (0 = to
 * end of buffer). `base` is the CPU address that rom[0] maps to, so branch
 * targets resolve to real addresses. Returns instructions decoded. */
size_t analyze_linear(const uint8_t *rom, size_t rom_size, uint16_t base,
                      size_t start_off, size_t count,
                      insn_cb cb, void *user);

/* ---- function discovery ---- */

#define MAX_FUNCS   16384

/* Discovery carves code into basic blocks: each block runs straight-line from
 * `start` to the first control-flow instruction (branch / JMP / JSR / RTS), or
 * to the next block's start if one falls inside it. Every inter-block transfer
 * is dispatched through the host driver (set vic_cpu.pc + return), so a JSR seeds
 * both its target and its return address as block entries, and a never-returning
 * loop ping-pongs through the driver instead of recursing in C. */
typedef struct {
    uint16_t start;                 /* first address                       */
    uint16_t end;                   /* one past the last instruction byte  */
    int      reaches_ret;           /* ends in an RTS/RTI                   */
    int      fallthrough;           /* ends with no terminator (split at a
                                     * leader): dispatch `end` to continue  */
    int      self_mod;              /* code is patched by a store - interpret it */
} func_t;

typedef struct {
    func_t   funcs[MAX_FUNCS];
    int      nfuncs;
    /* external call/jump targets (ROM monitor, DOS, slot I/O) - recorded only */
    uint16_t ext[MAX_FUNCS];
    int      next;
} func_table_t;

/* Recursive-descent discovery. Code image is `rom`/`rom_size` mapped at `base`.
 * Seeds are CPU addresses to start from (e.g. a binary's load address). Targets
 * outside [base, base+rom_size) are recorded as external, not followed. Returns
 * the number of functions discovered. */
int analyze_discover(const uint8_t *rom, size_t rom_size, uint16_t base,
                     const uint16_t *seeds, size_t nseeds,
                     func_table_t *out);

/* Find the function starting exactly at `addr`, or NULL. */
const func_t *func_table_find(const func_table_t *t, uint16_t addr);

/* Set after analyze_discover: bytes of recompiled code that an absolute store
 * writes (self-modified operand bytes). The emitter reads an instruction's
 * operand from memory at run time when its operand bytes are patched here. */
extern uint8_t analyze_patched[0x10000];

/* Restrict discovery to [code_lo, code_hi] (default full 0..0xFFFF). Targets
 * outside become external (interpreted) - essential for a 64K snapshot whose
 * ROM/IO region ($C000-$FFFF) is not the real ROM, so following calls into it
 * would recompile garbage. Set before analyze_discover. */
extern uint16_t analyze_code_lo, analyze_code_hi;

#endif /* M6502_ANALYZE_H */
