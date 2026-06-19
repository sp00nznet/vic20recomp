/* decode.h - MOS 6502 (NMOS) instruction decoder.
 *
 * The VIC-20 runs a stock NMOS MOS 6502 at ~1.02 MHz. It is the original,
 * pre-CMOS part: it has NO BRA / STZ / PHX / (zp) / RMBn / WAI, and instead the
 * unused encodings are the well-known "undocumented" opcodes (LAX, SAX, SLO,
 * DCP, ISC, ANC, the multi-byte NOPs, the JAM/KIL halts...). It also has the
 * famous JMP ($xxFF) indirect page-boundary bug, which the runtime models on
 * dispatch.
 *
 * This table is the complete 256-entry NMOS matrix: every documented opcode
 * plus every undocumented one, each with the correct addressing mode so a
 * linear sweep never desynchronizes on a real ROM image. This is the
 * foundation of the recompiler: decode -> analyze -> emit.
 */
#ifndef M6502_DECODE_H
#define M6502_DECODE_H

#include <stdint.h>

/* Addressing modes. Operand byte-count is derived from the mode. */
typedef enum {
    AM_IMP,   /* implied                         (1 byte)  */
    AM_ACC,   /* accumulator (e.g. ASL A)        (1 byte)  */
    AM_IMM,   /* #imm                            (2 bytes) */
    AM_ZP,    /* zp                              (2 bytes) */
    AM_ZPX,   /* zp,X                            (2 bytes) */
    AM_ZPY,   /* zp,Y                            (2 bytes) */
    AM_IZX,   /* (zp,X)                          (2 bytes) */
    AM_IZY,   /* (zp),Y                          (2 bytes) */
    AM_REL,   /* relative branch                 (2 bytes) */
    AM_ABS,   /* abs                             (3 bytes) */
    AM_ABX,   /* abs,X                           (3 bytes) */
    AM_ABY,   /* abs,Y                           (3 bytes) */
    AM_IND,   /* (abs)         JMP indirect      (3 bytes) */
    AM__COUNT
} addr_mode_t;

/* Control-flow class - drives function discovery in analyze.c. */
typedef enum {
    CF_NORMAL,  /* falls through                          */
    CF_BRANCH,  /* conditional branch (Bxx)               */
    CF_JMP,     /* unconditional jump (JMP)               */
    CF_CALL,    /* JSR                                    */
    CF_RET,     /* RTS / RTI                              */
    CF_BREAK,   /* BRK                                    */
    CF_STOP     /* JAM / KIL (undocumented CPU halt)      */
} cflow_t;

typedef struct {
    const char *mnemonic;
    uint8_t     mode;   /* addr_mode_t */
    uint8_t     cflow;  /* cflow_t     */
    uint8_t     undoc;  /* 1 = undocumented ("illegal") opcode */
} opinfo_t;

/* One decoded instruction. */
typedef struct {
    uint16_t pc;        /* address of the opcode byte           */
    uint8_t  opcode;    /* raw opcode                           */
    uint8_t  len;       /* total instruction length in bytes    */
    uint8_t  mode;      /* addr_mode_t                          */
    uint8_t  cflow;     /* cflow_t                              */
    uint8_t  undoc;     /* undocumented opcode                  */
    const char *mnemonic;
    uint16_t operand;   /* decoded operand value (addr or imm)  */
    uint16_t target;    /* resolved branch/jump target, else 0  */
} insn_t;

/* 256-entry opcode table, indexed by opcode byte. */
extern const opinfo_t m6502_optab[256];

/* Bytes consumed by an instruction of the given addressing mode. */
int m6502_mode_len(addr_mode_t mode);

/* Decode one instruction. `mem` points at the opcode byte; `pc` is its
 * address (used to resolve relative branch targets). Returns insn->len. */
int m6502_decode(const uint8_t *mem, uint16_t pc, insn_t *out);

/* Format a decoded instruction as a disassembly string (no address column). */
void m6502_format(const insn_t *in, char *buf, int bufsz);

#endif /* M6502_DECODE_H */
