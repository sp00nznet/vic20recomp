/* analyze.c - linear sweep + recursive-descent function discovery. See analyze.h. */
#include "analyze.h"
#include <string.h>

uint8_t analyze_patched[0x10000];
uint16_t analyze_code_lo = 0x0000, analyze_code_hi = 0xFFFF;

size_t analyze_linear(const uint8_t *rom, size_t rom_size, uint16_t base,
                      size_t start_off, size_t count,
                      insn_cb cb, void *user) {
    size_t off = start_off;
    size_t n   = 0;
    while (off < rom_size && (count == 0 || n < count)) {
        insn_t in;
        /* Guard the 1-2 trailing operand bytes against running off the end. */
        uint8_t tmp[3] = {0, 0, 0};
        size_t avail = rom_size - off;
        tmp[0] = rom[off];
        if (avail > 1) tmp[1] = rom[off + 1];
        if (avail > 2) tmp[2] = rom[off + 2];

        m6502_decode(tmp, (uint16_t)(base + off), &in);
        if (in.len > avail) break;     /* truncated final instruction */
        if (cb) cb(&in, user);
        off += in.len;
        n++;
    }
    return n;
}

/* ---- recursive-descent discovery ---- */

typedef struct {
    uint16_t       base;
    const uint8_t *rom;
    size_t         rom_size;
    uint16_t       work[MAX_FUNCS];
    int            nwork;
    uint8_t        seen[0x10000];   /* function-start seen bitmap */
} disc_t;

static int in_image(const disc_t *d, uint16_t a) {
    return a >= d->base && (size_t)(a - d->base) < d->rom_size &&
           a >= analyze_code_lo && a <= analyze_code_hi;
}

/* Read a 16-bit LE word from the image at CPU address `a` (0 if out of range). */
static uint16_t img_rd16(const disc_t *d, uint16_t a) {
    if (!in_image(d, a) || !in_image(d, (uint16_t)(a + 1))) return 0;
    size_t off = (size_t)(a - d->base);
    return (uint16_t)(d->rom[off] | (d->rom[off + 1] << 8));
}

static void push_func(disc_t *d, func_table_t *t, uint16_t a) {
    if (!in_image(d, a) || d->seen[a]) return;
    if (d->nwork >= MAX_FUNCS || t->nfuncs >= MAX_FUNCS) return;
    d->seen[a] = 1;
    d->work[d->nwork++] = a;
}

static void add_ext(func_table_t *t, uint16_t a) {
    for (int i = 0; i < t->next; i++) if (t->ext[i] == a) return;
    if (t->next < MAX_FUNCS) t->ext[t->next++] = a;
}

/* Decode a single basic block starting at leader `fs`: straight-line code up to
 * the first control-flow instruction. Each control transfer seeds its target(s)
 * - and a JSR/branch also seeds its fall-through/return address - as new block
 * leaders, so every inter-block edge can be dispatched through the host driver. */
static void discover_one(disc_t *d, func_table_t *t, uint16_t fs) {
    if (t->nfuncs >= MAX_FUNCS) return;
    func_t *f = &t->funcs[t->nfuncs];
    memset(f, 0, sizeof(*f));
    f->start = fs;

    uint16_t addr = fs;
    while (in_image(d, addr) && (uint16_t)(addr - fs) < 0x1000) {
        size_t off = (size_t)(addr - d->base);
        uint8_t tmp[3] = {0, 0, 0};
        size_t avail = d->rom_size - off;
        tmp[0] = d->rom[off];
        if (avail > 1) tmp[1] = d->rom[off + 1];
        if (avail > 2) tmp[2] = d->rom[off + 2];

        insn_t in;
        m6502_decode(tmp, addr, &in);
        if (in.len > avail) break;            /* truncated final instruction */
        uint16_t next = (uint16_t)(addr + in.len);

        switch (in.cflow) {
            case CF_CALL:                     /* JSR: seed target + return addr */
                if (in_image(d, in.target)) push_func(d, t, in.target);
                else                        add_ext(t, in.target);
                push_func(d, t, next);
                f->end = next; t->nfuncs++; return;
            case CF_BRANCH:                   /* seed taken target + fall-through */
                if (in_image(d, in.target)) push_func(d, t, in.target);
                push_func(d, t, next);
                f->end = next; t->nfuncs++; return;
            case CF_JMP:
                if (in.mode == AM_IND) {
                    uint16_t tgt = img_rd16(d, in.operand);
                    if (in_image(d, tgt)) push_func(d, t, tgt);
                } else if (in.target != 0 && in_image(d, in.target)) {
                    push_func(d, t, in.target);
                } else if (in.target != 0) {
                    add_ext(t, in.target);
                }
                f->end = next; t->nfuncs++; return;
            case CF_RET:
                f->reaches_ret = 1;
                f->end = next; t->nfuncs++; return;
            case CF_BREAK:
            case CF_STOP:
                f->end = next; t->nfuncs++; return;
            default:
                break;
        }
        addr = next;
    }

    /* ran off the image or hit the size cap with no terminator: the block falls
     * through to whatever is at `addr` (the emitter dispatches it). */
    f->end = addr;
    f->fallthrough = 1;
    t->nfuncs++;
}

/* A branch/jump target can land inside an already-formed straight-line block
 * (e.g. a loop body whose head is branched to from its tail). Split such a block
 * at the interior leader: it falls through to the leader's own block. Only split
 * at a real instruction boundary - a leader that lands mid-instruction means
 * this block decoded data as code (or that target is into data); cutting there
 * would emit a block ending mid-instruction with a bogus fall-through. */
static void split_at_leaders(const disc_t *d, func_table_t *t) {
    for (int i = 0; i < t->nfuncs; i++) {
        func_t *f = &t->funcs[i];
        uint16_t a = f->start;
        while (a < f->end) {
            size_t off = (size_t)(a - d->base);
            if (off >= d->rom_size) break;
            uint8_t tmp[3] = { d->rom[off],
                               (off + 1 < d->rom_size) ? d->rom[off + 1] : 0,
                               (off + 2 < d->rom_size) ? d->rom[off + 2] : 0 };
            insn_t in; m6502_decode(tmp, a, &in);
            uint16_t next = (uint16_t)(a + (in.len ? in.len : 1));
            if (next < f->end && d->seen[next]) {   /* leader at a real boundary */
                f->end = next;
                f->fallthrough = 1;
                f->reaches_ret = 0;                 /* terminator now lives past the cut */
                break;
            }
            a = next;
        }
    }
}

int analyze_discover(const uint8_t *rom, size_t rom_size, uint16_t base,
                     const uint16_t *seeds, size_t nseeds,
                     func_table_t *out) {
    static disc_t d;                      /* large (seen[]); keep off stack */
    memset(&d, 0, sizeof(d));
    d.base = base; d.rom = rom; d.rom_size = rom_size;
    memset(out, 0, sizeof(*out));
    memset(analyze_patched, 0, sizeof(analyze_patched));

    for (size_t i = 0; i < nseeds; i++) push_func(&d, out, seeds[i]);

    while (d.nwork > 0)
        discover_one(&d, out, d.work[--d.nwork]);

    split_at_leaders(&d, out);

    /* Self-modifying code. Scan stores into recompiled code:
     *  - absolute store (STA/STX/STY $addr): records the patched byte; if it is
     *    an instruction *operand* byte the emitter reads that operand from memory
     *    at run time, so the function still recompiles natively.
     *  - indexed store into code (STA $addr,X/,Y): the patched byte can't be
     *    pinned down, so the target function is marked self-modifying and left
     *    to the interpreter.
     * A second pass marks a function self-modifying if a patched byte is an
     * instruction *opcode* or a control-flow operand (those can't be made
     * operand-from-memory safely). */
    for (int i = 0; i < out->nfuncs; i++) {
        func_t *f = &out->funcs[i];
        for (uint16_t a = f->start; a < f->end && in_image(&d, a); ) {
            size_t off = (size_t)(a - base);
            uint8_t tmp[3] = { rom[off], (off+1<rom_size)?rom[off+1]:0, (off+2<rom_size)?rom[off+2]:0 };
            insn_t in; m6502_decode(tmp, a, &in);
            int is_store = in.mnemonic[0]=='S' && in.mnemonic[1]=='T' &&
                (in.mnemonic[2]=='A'||in.mnemonic[2]=='X'||in.mnemonic[2]=='Y');
            if (is_store) {
                int in_code = func_table_find(out, in.operand) != NULL;
                for (int g = 0; g < out->nfuncs; g++)
                    if (in.operand >= out->funcs[g].start && in.operand < out->funcs[g].end) in_code = 1;
                if (in_code) {
                    if (in.mode == AM_ABS) analyze_patched[in.operand] = 1;
                    else if (in.mode == AM_ABX || in.mode == AM_ABY) {
                        for (int g = 0; g < out->nfuncs; g++)
                            if (in.operand >= out->funcs[g].start && in.operand < out->funcs[g].end)
                                out->funcs[g].self_mod = 1;
                    }
                }
            }
            if (in.len == 0) break;
            a = (uint16_t)(a + in.len);
            if (a <= f->start) break;
        }
    }
    /* Pass 2: a patched opcode byte, or a patched control-flow operand, forces
     * interpretation of the containing function. */
    for (int i = 0; i < out->nfuncs; i++) {
        func_t *f = &out->funcs[i];
        for (uint16_t a = f->start; a < f->end && in_image(&d, a); ) {
            size_t off = (size_t)(a - base);
            uint8_t tmp[3] = { rom[off], (off+1<rom_size)?rom[off+1]:0, (off+2<rom_size)?rom[off+2]:0 };
            insn_t in; m6502_decode(tmp, a, &in);
            if (analyze_patched[a]) f->self_mod = 1;                 /* opcode patched */
            if (in.cflow != CF_NORMAL && in.cflow != CF_RET &&
                (analyze_patched[(uint16_t)(a+1)] || analyze_patched[(uint16_t)(a+2)]))
                f->self_mod = 1;                                     /* control-flow operand patched */
            if (in.len == 0) break;
            a = (uint16_t)(a + in.len);
            if (a <= f->start) break;
        }
    }

    return out->nfuncs;
}

const func_t *func_table_find(const func_table_t *t, uint16_t addr) {
    for (int i = 0; i < t->nfuncs; i++)
        if (t->funcs[i].start == addr) return &t->funcs[i];
    return NULL;
}
