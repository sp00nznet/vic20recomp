/* mem.c - Commodore VIC-20 flat address space. See mem.h.
 *
 * On the VIC-20 the picture is RAM the VIC chip scans, the VIC/VIA registers are
 * just addresses, and the cartridge ROM is mapped image. So the machine is a
 * single flat 64K array as far as recompiled code is concerned - no banking, no
 * soft switches. A host may intercept the $90xx/$91xx I/O page through
 * vic_io_hook (to model VIA timers / inputs); by default it is plain memory. */
#include "vic20recomp/mem.h"
#include "vic20recomp/cpu.h"
#include <string.h>

uint8_t vic_ram[VIC_RAM_SIZE];

uint8_t (*vic_io_hook)(uint16_t addr, int is_write, uint8_t val) = 0;

void vic_mem_init(void) {
    memset(vic_ram, 0, sizeof(vic_ram));
}

/* The VIC register state a KERNAL cold start (CINT) leaves on an NTSC machine:
 * screen matrix $1E00, character base $8000, 22x23 8x8 text, white background,
 * cyan border, normal (non-reverse) video. A directly booted cart that does not
 * re-run CINT relies on these. (A cart that drives the VIC itself overwrites
 * them - the readout always reflects the live registers.) */
void vic_set_default_regs(void) {
    vic_ram[0x9000] = 0x0C;   /* horizontal origin                       */
    vic_ram[0x9001] = 0x26;   /* vertical origin                         */
    vic_ram[0x9002] = 0x96;   /* bit7 = screen addr bit9; bits0-6 = 22 cols */
    vic_ram[0x9003] = 0x2E;   /* bits1-6 = 23 rows; bit0 = 8x8 chars     */
    vic_ram[0x9004] = 0x00;   /* raster (read-only on hw)                */
    vic_ram[0x9005] = 0xF0;   /* screen $1E00, charset $8000             */
    vic_ram[0x900E] = 0x00;   /* aux colour (hi nibble) + volume         */
    vic_ram[0x900F] = 0x1B;   /* bg=white(1), normal video, border=cyan(3) */
}

uint8_t vic_mem_read(uint16_t addr) {
    if (vic_io_hook && addr >= 0x9000 && addr < 0xA000)
        return vic_io_hook(addr, 0, 0);
    return vic_ram[addr];
}

void vic_mem_write(uint16_t addr, uint8_t val) {
    if (vic_io_hook && addr >= 0x9000 && addr < 0xA000)
        vic_io_hook(addr, 1, val);   /* hook may observe; store falls through */
    vic_ram[addr] = val;
}

uint16_t vic_mem_read16(uint16_t addr) {
    return (uint16_t)(vic_mem_read(addr) |
                      (vic_mem_read((uint16_t)(addr + 1)) << 8));
}

uint16_t vic_mem_read16_bug(uint16_t addr) {
    /* NMOS JMP ($xxFF): the high byte comes from $xx00, same page. */
    uint16_t hi_addr = (uint16_t)((addr & 0xFF00) | ((addr + 1) & 0x00FF));
    return (uint16_t)(vic_mem_read(addr) | (vic_mem_read(hi_addr) << 8));
}
