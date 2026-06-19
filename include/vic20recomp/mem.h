/* mem.h - Commodore VIC-20 address space + memory access for recompiled code.
 *
 * The MOS 6502 sees a flat 64 KiB space. The unexpanded VIC-20 layout:
 *
 *   $0000-$00FF  zero page
 *   $0100-$01FF  CPU stack
 *   $0200-$03FF  KERNAL/BASIC work area + the RAM IRQ/NMI vectors ($0314..)
 *   $1000-$1FFF  4K user RAM (default screen at $1E00, 22x23 text, 506 bytes)
 *   $8000-$8FFF  CHARGEN character ROM (bring-your-own; absent here)
 *   $9000-$900F  VIC (6560/6561) registers
 *   $9110-$912F  VIA #1 / #2 (timers, keyboard, joystick)
 *   $9400-$95FF  colour RAM (expanded)   - low nibble per cell
 *   $9600-$97FF  colour RAM (unexpanded) - low nibble per cell
 *   $A000-$BFFF  cartridge ROM (autostart; cold-start vector word at $A000)
 *   $C000-$DFFF  BASIC ROM (bring-your-own)
 *   $E000-$FFFF  KERNAL ROM (bring-your-own)
 *
 * The VIC-20's picture is a region of RAM the VIC chip scans, exactly like the
 * Apple II hi-res scan or the Pac-Man tile grid - so the whole machine is just a
 * flat RAM array as far as the recompiled code is concerned. Cartridge ROM,
 * screen RAM, colour RAM and the VIC registers all live in vic_ram and the video
 * readout reads them back. A host may intercept the $90xx/$91xx I/O page through
 * vic_io_hook (e.g. to model the VIA timers that drive the 60 Hz IRQ); by default
 * everything is plain flat memory, which is all a static frame render needs. */
#ifndef VIC20RECOMP_MEM_H
#define VIC20RECOMP_MEM_H

#include <stdint.h>
#include <stddef.h>

#define VIC_RAM_SIZE   0x10000u

/* 6502 hardware vectors. */
#define VIC_VEC_NMI    0xFFFAu
#define VIC_VEC_RESET  0xFFFCu
#define VIC_VEC_IRQ    0xFFFEu

/* The RAM vectors the KERNAL indirects through (cart games usually install their
 * IRQ here so the host can fire it). */
#define VIC_VEC_CINV   0x0314u   /* IRQ/BRK indirect */
#define VIC_VEC_CNBINV 0x0318u   /* NMI indirect     */

/* Cartridge autostart region + default unexpanded layout. */
#define VIC_CART_BASE      0xA000u
#define VIC_CART_END       0xC000u
#define VIC_SCREEN_DEFAULT 0x1E00u
#define VIC_COLOR_DEFAULT  0x9600u
#define VIC_REG_BASE       0x9000u   /* $9000-$900F VIC chip registers */

extern uint8_t vic_ram[VIC_RAM_SIZE];

/* Optional host interception of the I/O page ($9000-$9FFF). Return value is used
 * for reads; ignored for writes (the store still lands in RAM so the VIC
 * registers and colour RAM stay readable by the video readout). NULL (default) =
 * plain flat memory, which is all a static frame render needs. */
extern uint8_t (*vic_io_hook)(uint16_t addr, int is_write, uint8_t val);

uint8_t vic_mem_read(uint16_t addr);
void    vic_mem_write(uint16_t addr, uint8_t val);

/* 16-bit little-endian read (correct, used by indirect addressing). */
uint16_t vic_mem_read16(uint16_t addr);

/* 16-bit read with the NMOS JMP ($xxFF) page-wrap bug: the high byte is fetched
 * from $xx00, not $(xx+1)00. Used only by the recompiled JMP (abs). */
uint16_t vic_mem_read16_bug(uint16_t addr);

void vic_mem_init(void);

/* Write the VIC register defaults a KERNAL cold start (CINT) would leave: screen
 * at $1E00, charset at $8000, 22x23 text, white background, cyan border. A cart
 * booted directly (no KERNAL) relies on these being present before it runs. */
void vic_set_default_regs(void);

#endif /* VIC20RECOMP_MEM_H */
