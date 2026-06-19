/* a2regs.h - VIC-20 hardware-register + KERNAL-entry naming for disassembly.
 *
 * Annotating operands turns raw `STA $900F` into `STA $900F ; VIC_COLORS
 * (bg/border colour)` and `JSR $FFD2` into `JSR $FFD2 ; CHROUT (output char)`.
 * This is the VIC-20's hardware-register map: the VIC chip ($9000-$900F), the
 * VIAs ($9110-$912F), colour RAM, and the well-known KERNAL jump-table entries a
 * cartridge calls. (The file name is historical - the recompiler front-end is
 * shared with the Apple II 6502 toolkit.)
 */
#ifndef M6502_A2REGS_H
#define M6502_A2REGS_H

#include <stdint.h>

/* Short name for a well-known VIC-20 address, or NULL. */
const char *vic_reg_name(uint16_t addr);
/* One-line description for an address, or NULL. */
const char *vic_reg_note(uint16_t addr);

#endif /* M6502_A2REGS_H */
