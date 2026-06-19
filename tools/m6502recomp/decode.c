/* decode.c - NMOS MOS 6502 opcode table + decoder. See decode.h. */
#include "decode.h"
#include <stdio.h>
#include <string.h>

#define N  CF_NORMAL
#define BR CF_BRANCH
#define JP CF_JMP
#define CL CF_CALL
#define RT CF_RET
#define BK CF_BREAK
#define SP CF_STOP

/* Documented opcode: {mnemonic, mode, cflow, 0}. */
#define D(M,MODE,CF) {M, MODE, CF, 0}
/* Undocumented ("illegal") opcode: {mnemonic, mode, cflow, 1}. */
#define U(M,MODE,CF) {M, MODE, CF, 1}

/* The complete 256-entry NMOS 6502 matrix. Documented opcodes plus the stable
 * undocumented set; the JAM/KIL encodings ($x2 column, mostly) halt the CPU.
 * Multi-byte NOPs are decoded with their true widths so a linear sweep over a
 * disk image never loses instruction sync on embedded data. */
const opinfo_t m6502_optab[256] = {
/*         x0               x1               x2               x3               x4               x5               x6               x7               x8               x9               xA               xB               xC               xD               xE               xF        */
/*0x*/ D("BRK",AM_IMP,BK), D("ORA",AM_IZX,N), U("JAM",AM_IMP,SP), U("SLO",AM_IZX,N), U("NOP",AM_ZP,N),  D("ORA",AM_ZP,N),  D("ASL",AM_ZP,N),  U("SLO",AM_ZP,N),  D("PHP",AM_IMP,N), D("ORA",AM_IMM,N), D("ASL",AM_ACC,N), U("ANC",AM_IMM,N), U("NOP",AM_ABS,N), D("ORA",AM_ABS,N), D("ASL",AM_ABS,N), U("SLO",AM_ABS,N),
/*1x*/ D("BPL",AM_REL,BR), D("ORA",AM_IZY,N), U("JAM",AM_IMP,SP), U("SLO",AM_IZY,N), U("NOP",AM_ZPX,N), D("ORA",AM_ZPX,N), D("ASL",AM_ZPX,N), U("SLO",AM_ZPX,N), D("CLC",AM_IMP,N), D("ORA",AM_ABY,N), U("NOP",AM_IMP,N), U("SLO",AM_ABY,N), U("NOP",AM_ABX,N), D("ORA",AM_ABX,N), D("ASL",AM_ABX,N), U("SLO",AM_ABX,N),
/*2x*/ D("JSR",AM_ABS,CL), D("AND",AM_IZX,N), U("JAM",AM_IMP,SP), U("RLA",AM_IZX,N), D("BIT",AM_ZP,N),  D("AND",AM_ZP,N),  D("ROL",AM_ZP,N),  U("RLA",AM_ZP,N),  D("PLP",AM_IMP,N), D("AND",AM_IMM,N), D("ROL",AM_ACC,N), U("ANC",AM_IMM,N), D("BIT",AM_ABS,N), D("AND",AM_ABS,N), D("ROL",AM_ABS,N), U("RLA",AM_ABS,N),
/*3x*/ D("BMI",AM_REL,BR), D("AND",AM_IZY,N), U("JAM",AM_IMP,SP), U("RLA",AM_IZY,N), U("NOP",AM_ZPX,N), D("AND",AM_ZPX,N), D("ROL",AM_ZPX,N), U("RLA",AM_ZPX,N), D("SEC",AM_IMP,N), D("AND",AM_ABY,N), U("NOP",AM_IMP,N), U("RLA",AM_ABY,N), U("NOP",AM_ABX,N), D("AND",AM_ABX,N), D("ROL",AM_ABX,N), U("RLA",AM_ABX,N),
/*4x*/ D("RTI",AM_IMP,RT), D("EOR",AM_IZX,N), U("JAM",AM_IMP,SP), U("SRE",AM_IZX,N), U("NOP",AM_ZP,N),  D("EOR",AM_ZP,N),  D("LSR",AM_ZP,N),  U("SRE",AM_ZP,N),  D("PHA",AM_IMP,N), D("EOR",AM_IMM,N), D("LSR",AM_ACC,N), U("ALR",AM_IMM,N), D("JMP",AM_ABS,JP), D("EOR",AM_ABS,N), D("LSR",AM_ABS,N), U("SRE",AM_ABS,N),
/*5x*/ D("BVC",AM_REL,BR), D("EOR",AM_IZY,N), U("JAM",AM_IMP,SP), U("SRE",AM_IZY,N), U("NOP",AM_ZPX,N), D("EOR",AM_ZPX,N), D("LSR",AM_ZPX,N), U("SRE",AM_ZPX,N), D("CLI",AM_IMP,N), D("EOR",AM_ABY,N), U("NOP",AM_IMP,N), U("SRE",AM_ABY,N), U("NOP",AM_ABX,N), D("EOR",AM_ABX,N), D("LSR",AM_ABX,N), U("SRE",AM_ABX,N),
/*6x*/ D("RTS",AM_IMP,RT), D("ADC",AM_IZX,N), U("JAM",AM_IMP,SP), U("RRA",AM_IZX,N), U("NOP",AM_ZP,N),  D("ADC",AM_ZP,N),  D("ROR",AM_ZP,N),  U("RRA",AM_ZP,N),  D("PLA",AM_IMP,N), D("ADC",AM_IMM,N), D("ROR",AM_ACC,N), U("ARR",AM_IMM,N), D("JMP",AM_IND,JP), D("ADC",AM_ABS,N), D("ROR",AM_ABS,N), U("RRA",AM_ABS,N),
/*7x*/ D("BVS",AM_REL,BR), D("ADC",AM_IZY,N), U("JAM",AM_IMP,SP), U("RRA",AM_IZY,N), U("NOP",AM_ZPX,N), D("ADC",AM_ZPX,N), D("ROR",AM_ZPX,N), U("RRA",AM_ZPX,N), D("SEI",AM_IMP,N), D("ADC",AM_ABY,N), U("NOP",AM_IMP,N), U("RRA",AM_ABY,N), U("NOP",AM_ABX,N), D("ADC",AM_ABX,N), D("ROR",AM_ABX,N), U("RRA",AM_ABX,N),
/*8x*/ U("NOP",AM_IMM,N), D("STA",AM_IZX,N), U("NOP",AM_IMM,N), U("SAX",AM_IZX,N), D("STY",AM_ZP,N),  D("STA",AM_ZP,N),  D("STX",AM_ZP,N),  U("SAX",AM_ZP,N),  D("DEY",AM_IMP,N), U("NOP",AM_IMM,N), D("TXA",AM_IMP,N), U("ANE",AM_IMM,N), D("STY",AM_ABS,N), D("STA",AM_ABS,N), D("STX",AM_ABS,N), U("SAX",AM_ABS,N),
/*9x*/ D("BCC",AM_REL,BR), D("STA",AM_IZY,N), U("JAM",AM_IMP,SP), U("SHA",AM_IZY,N), D("STY",AM_ZPX,N), D("STA",AM_ZPX,N), D("STX",AM_ZPY,N), U("SAX",AM_ZPY,N), D("TYA",AM_IMP,N), D("STA",AM_ABY,N), D("TXS",AM_IMP,N), U("TAS",AM_ABY,N), U("SHY",AM_ABX,N), D("STA",AM_ABX,N), U("SHX",AM_ABY,N), U("SHA",AM_ABY,N),
/*Ax*/ D("LDY",AM_IMM,N), D("LDA",AM_IZX,N), D("LDX",AM_IMM,N), U("LAX",AM_IZX,N), D("LDY",AM_ZP,N),  D("LDA",AM_ZP,N),  D("LDX",AM_ZP,N),  U("LAX",AM_ZP,N),  D("TAY",AM_IMP,N), D("LDA",AM_IMM,N), D("TAX",AM_IMP,N), U("LXA",AM_IMM,N), D("LDY",AM_ABS,N), D("LDA",AM_ABS,N), D("LDX",AM_ABS,N), U("LAX",AM_ABS,N),
/*Bx*/ D("BCS",AM_REL,BR), D("LDA",AM_IZY,N), U("JAM",AM_IMP,SP), U("LAX",AM_IZY,N), D("LDY",AM_ZPX,N), D("LDA",AM_ZPX,N), D("LDX",AM_ZPY,N), U("LAX",AM_ZPY,N), D("CLV",AM_IMP,N), D("LDA",AM_ABY,N), D("TSX",AM_IMP,N), U("LAS",AM_ABY,N), D("LDY",AM_ABX,N), D("LDA",AM_ABX,N), D("LDX",AM_ABY,N), U("LAX",AM_ABY,N),
/*Cx*/ D("CPY",AM_IMM,N), D("CMP",AM_IZX,N), U("NOP",AM_IMM,N), U("DCP",AM_IZX,N), D("CPY",AM_ZP,N),  D("CMP",AM_ZP,N),  D("DEC",AM_ZP,N),  U("DCP",AM_ZP,N),  D("INY",AM_IMP,N), D("CMP",AM_IMM,N), D("DEX",AM_IMP,N), U("SBX",AM_IMM,N), D("CPY",AM_ABS,N), D("CMP",AM_ABS,N), D("DEC",AM_ABS,N), U("DCP",AM_ABS,N),
/*Dx*/ D("BNE",AM_REL,BR), D("CMP",AM_IZY,N), U("JAM",AM_IMP,SP), U("DCP",AM_IZY,N), U("NOP",AM_ZPX,N), D("CMP",AM_ZPX,N), D("DEC",AM_ZPX,N), U("DCP",AM_ZPX,N), D("CLD",AM_IMP,N), D("CMP",AM_ABY,N), U("NOP",AM_IMP,N), U("DCP",AM_ABY,N), U("NOP",AM_ABX,N), D("CMP",AM_ABX,N), D("DEC",AM_ABX,N), U("DCP",AM_ABX,N),
/*Ex*/ D("CPX",AM_IMM,N), D("SBC",AM_IZX,N), U("NOP",AM_IMM,N), U("ISC",AM_IZX,N), D("CPX",AM_ZP,N),  D("SBC",AM_ZP,N),  D("INC",AM_ZP,N),  U("ISC",AM_ZP,N),  D("INX",AM_IMP,N), D("SBC",AM_IMM,N), D("NOP",AM_IMP,N), U("USB",AM_IMM,N), D("CPX",AM_ABS,N), D("SBC",AM_ABS,N), D("INC",AM_ABS,N), U("ISC",AM_ABS,N),
/*Fx*/ D("BEQ",AM_REL,BR), D("SBC",AM_IZY,N), U("JAM",AM_IMP,SP), U("ISC",AM_IZY,N), U("NOP",AM_ZPX,N), D("SBC",AM_ZPX,N), D("INC",AM_ZPX,N), U("ISC",AM_ZPX,N), D("SED",AM_IMP,N), D("SBC",AM_ABY,N), U("NOP",AM_IMP,N), U("ISC",AM_ABY,N), U("NOP",AM_ABX,N), D("SBC",AM_ABX,N), D("INC",AM_ABX,N), U("ISC",AM_ABX,N),
};

int m6502_mode_len(addr_mode_t mode) {
    switch (mode) {
        case AM_IMP: case AM_ACC:
            return 1;
        case AM_IMM: case AM_ZP: case AM_ZPX: case AM_ZPY:
        case AM_IZX: case AM_IZY: case AM_REL:
            return 2;
        case AM_ABS: case AM_ABX: case AM_ABY: case AM_IND:
            return 3;
        default:
            return 1;
    }
}

int m6502_decode(const uint8_t *mem, uint16_t pc, insn_t *out) {
    uint8_t op = mem[0];
    const opinfo_t *oi = &m6502_optab[op];

    memset(out, 0, sizeof(*out));
    out->pc       = pc;
    out->opcode   = op;
    out->mode     = oi->mode;
    out->cflow    = oi->cflow;
    out->undoc    = oi->undoc;
    out->mnemonic = oi->mnemonic;
    out->len      = (uint8_t)m6502_mode_len((addr_mode_t)oi->mode);

    switch (oi->mode) {
        case AM_IMM: case AM_ZP: case AM_ZPX: case AM_ZPY:
        case AM_IZX: case AM_IZY:
            out->operand = mem[1];
            break;
        case AM_REL: {
            int8_t d = (int8_t)mem[1];
            out->operand = mem[1];
            out->target  = (uint16_t)(pc + 2 + d);
            break;
        }
        case AM_ABS: case AM_ABX: case AM_ABY: case AM_IND:
            out->operand = (uint16_t)(mem[1] | (mem[2] << 8));
            if (oi->mode == AM_ABS &&
                (oi->cflow == CF_JMP || oi->cflow == CF_CALL))
                out->target = out->operand;
            break;
        default:
            break;
    }
    return out->len;
}

void m6502_format(const insn_t *in, char *buf, int bufsz) {
    const char *m = in->mnemonic;
    switch (in->mode) {
        case AM_IMP:   snprintf(buf, bufsz, "%s", m); break;
        case AM_ACC:   snprintf(buf, bufsz, "%s A", m); break;
        case AM_IMM:   snprintf(buf, bufsz, "%s #$%02X", m, in->operand); break;
        case AM_ZP:    snprintf(buf, bufsz, "%s $%02X", m, in->operand); break;
        case AM_ZPX:   snprintf(buf, bufsz, "%s $%02X,X", m, in->operand); break;
        case AM_ZPY:   snprintf(buf, bufsz, "%s $%02X,Y", m, in->operand); break;
        case AM_IZX:   snprintf(buf, bufsz, "%s ($%02X,X)", m, in->operand); break;
        case AM_IZY:   snprintf(buf, bufsz, "%s ($%02X),Y", m, in->operand); break;
        case AM_REL:   snprintf(buf, bufsz, "%s $%04X", m, in->target); break;
        case AM_ABS:   snprintf(buf, bufsz, "%s $%04X", m, in->operand); break;
        case AM_ABX:   snprintf(buf, bufsz, "%s $%04X,X", m, in->operand); break;
        case AM_ABY:   snprintf(buf, bufsz, "%s $%04X,Y", m, in->operand); break;
        case AM_IND:   snprintf(buf, bufsz, "%s ($%04X)", m, in->operand); break;
        default:       snprintf(buf, bufsz, "%s ?", m); break;
    }
}
