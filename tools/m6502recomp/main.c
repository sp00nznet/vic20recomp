/* m6502recomp - VIC-20 static-recompiler driver.
 *
 * Subcommands:
 *   disbin   <raw.bin> <baseHex> [count]    disassemble a raw image at base
 *   recompbin<raw.bin> <baseHex> <outdir> [seedHex...] [--code LO HI]
 *                                          [--seeds file] [--table ADDR COUNT ADJ]
 *               discover reachable basic blocks from the seeds and emit one C
 *               function per block to <outdir>/recomp_funcs.{c,h}.
 *
 * A VIC-20 cartridge is a raw ROM mapped at its load address (usually $A000).
 * The cold-start vector is the word at $A000; seed discovery there (the host
 * passes it, plus any JSR-target seeds captured by an interpreter run). The
 * decoder / analyzer / emitter are the shared 6502 front-end. See README.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode.h"
#include "analyze.h"
#include "emit.h"
#include "a2regs.h"

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *out_size = (size_t)n;
    return buf;
}

static void dis_cb(const insn_t *in, void *user) {
    (void)user;
    char text[48];
    m6502_format(in, text, sizeof(text));
    const char *reg = NULL;
    if (in->mode == AM_ABS || in->mode == AM_ABX || in->mode == AM_ABY ||
        in->mode == AM_IND)
        reg = vic_reg_name((uint16_t)in->operand);
    const char *flag = in->undoc ? "*" : " ";
    if (reg) {
        const char *note = vic_reg_note((uint16_t)in->operand);
        if (note) printf("%04X %s %02X %-18s ; %s (%s)\n", in->pc, flag, in->opcode, text, reg, note);
        else      printf("%04X %s %02X %-18s ; %s\n", in->pc, flag, in->opcode, text, reg);
    } else {
        printf("%04X %s %02X %s\n", in->pc, flag, in->opcode, text);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
            "m6502recomp - VIC-20 static recompiler\n"
            "usage:\n"
            "  %s disbin   <raw.bin> <baseHex> [count]\n"
            "  %s recompbin<raw.bin> <baseHex> <outdir> [seedHex...] [--code LO HI]\n"
            "                                            [--seeds file] [--table ADDR COUNT ADJ]\n",
            argv[0], argv[0]);
        return 2;
    }

    const char *cmd  = argv[1];
    const char *path = argv[2];

    size_t size = 0;
    uint8_t *data = read_file(path, &size);
    if (!data) { fprintf(stderr, "error: cannot read %s\n", path); return 1; }

    int rc = 0;

    if (strcmp(cmd, "disbin") == 0) {
        uint16_t base = (argc > 3) ? (uint16_t)strtoul(argv[3], NULL, 0) : 0;
        size_t count  = (argc > 4) ? (size_t)strtoul(argv[4], NULL, 0) : 64;
        printf("; linear sweep of %s, base $%04X (* = undocumented)\n", path, base);
        analyze_linear(data, size, base, 0, count, dis_cb, NULL);
        free(data);
        return 0;
    }
    if (strcmp(cmd, "recompbin") == 0) {
        if (argc < 5) { fprintf(stderr, "error: recompbin needs <baseHex> <outdir>\n"); free(data); return 2; }
        uint16_t base = (uint16_t)strtoul(argv[3], NULL, 0);
        static func_table_t tab;
        static uint16_t seeds[1024]; size_t ns = 0;
        /* Seeds are CPU addresses, plus optional forms:
         *   --code LO HI       restrict discovery to [LO,HI] (others -> external)
         *   --seeds file       read hex addresses (one per line; JSR targets a
         *                      runner captured - ideal for self-modifying code)
         *   --table ADDR N ADJ read N little-endian word entries at ADDR and seed
         *                      each (entry + ADJ); recovers handlers reached
         *                      through an address / jump table. */
        for (int i = 5; i < argc && ns < 1000; i++) {
            if (strcmp(argv[i], "--code") == 0 && i + 2 < argc) {
                analyze_code_lo = (uint16_t)strtoul(argv[i+1], NULL, 0);
                analyze_code_hi = (uint16_t)strtoul(argv[i+2], NULL, 0);
                i += 2;
            } else if (strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) {
                FILE *sf = fopen(argv[i+1], "r");
                if (sf) {
                    char line[64];
                    while (ns < 1000 && fgets(line, sizeof(line), sf))
                        if (line[0]=='0'||line[0]=='$'||((line[0]>='A')&&(line[0]<='F'))||((line[0]>='0')&&(line[0]<='9')))
                            seeds[ns++] = (uint16_t)strtoul(line[0]=='$'?line+1:line, NULL, 0);
                    fclose(sf);
                }
                i += 1;
            } else if (strcmp(argv[i], "--table") == 0 && i + 3 < argc) {
                uint16_t taddr = (uint16_t)strtoul(argv[i+1], NULL, 0);
                long cnt = strtol(argv[i+2], NULL, 0);
                long adj = strtol(argv[i+3], NULL, 0);
                for (long e = 0; e < cnt && ns < 1000; e++) {
                    size_t off = (size_t)(taddr - base) + (size_t)e * 2;
                    if (off + 1 >= size) break;
                    uint16_t v = (uint16_t)(data[off] | (data[off+1] << 8));
                    v = (uint16_t)(v + adj);
                    if (v >= base && (size_t)(v - base) < size) seeds[ns++] = v;
                }
                i += 3;
            } else {
                seeds[ns++] = (uint16_t)strtoul(argv[i], NULL, 0);
            }
        }
        if (ns == 0) seeds[ns++] = base;     /* default: entry at base */
        int nf = analyze_discover(data, size, base, seeds, ns, &tab);
        if (emit_functions(argv[4], data, size, base, &tab, path) != 0) {
            fprintf(stderr, "error: emit failed\n"); rc = 1;
        } else {
            printf("recompiled %d functions (%zu bytes @ $%04X) -> %s\n",
                   nf, size, base, argv[4]);
            printf("  external targets: %d\n", tab.next);
        }
        free(data);
        return rc;
    }

    fprintf(stderr, "error: unknown command '%s'\n", cmd);
    free(data);
    return 2;
}
