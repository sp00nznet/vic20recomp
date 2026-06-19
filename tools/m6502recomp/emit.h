/* emit.h - C emission for the 6502 recompiler. */
#ifndef M6502_EMIT_H
#define M6502_EMIT_H

#include <stddef.h>
#include <stdint.h>
#include "analyze.h"

/* Emit one readable C function per discovered routine into
 * <outdir>/recomp_funcs.{c,h}. `rom` is the loaded code image mapped at `base`.
 * `rom_label` is recorded in a header comment. Returns 0 on success. */
int emit_functions(const char *outdir, const uint8_t *rom, size_t rom_size,
                   uint16_t base, const func_table_t *t, const char *rom_label);

/* Emit an empty recomp_funcs.{c,h} skeleton (no functions). */
int emit_skeleton(const char *outdir, const char *rom_label);

#endif /* M6502_EMIT_H */
