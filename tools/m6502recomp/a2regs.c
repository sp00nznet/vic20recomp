/* a2regs.c - VIC-20 hardware-register + KERNAL-entry naming. See a2regs.h. */
#include "a2regs.h"
#include <stddef.h>

typedef struct { uint16_t addr; const char *name; const char *note; } a2reg_t;

/* VIC chip ($9000-$900F) + VIA #1/#2 ($9110-$912F) + colour RAM. */
static const a2reg_t io_page[] = {
    {0x9000, "VIC_HORIG", "horizontal origin / interlace"},
    {0x9001, "VIC_VORIG", "vertical origin"},
    {0x9002, "VIC_COLS",  "screen addr bit9 + # columns"},
    {0x9003, "VIC_ROWS",  "# rows + 8x8/8x16 char size"},
    {0x9004, "VIC_RASTER","raster line (read)"},
    {0x9005, "VIC_MEM",   "screen + character memory base"},
    {0x9006, "VIC_LPENX", "light pen X"},
    {0x9007, "VIC_LPENY", "light pen Y"},
    {0x9008, "VIC_PADX",  "paddle X"},
    {0x9009, "VIC_PADY",  "paddle Y"},
    {0x900A, "VIC_OSC1",  "bass oscillator"},
    {0x900B, "VIC_OSC2",  "alto oscillator"},
    {0x900C, "VIC_OSC3",  "soprano oscillator"},
    {0x900D, "VIC_NOISE", "noise oscillator"},
    {0x900E, "VIC_AUXVOL","aux colour + volume"},
    {0x900F, "VIC_COLORS","background + border colour"},
    {0x9110, "VIA1_PB",   "VIA1 port B (user port)"},
    {0x9111, "VIA1_PA",   "VIA1 port A (joystick/serial)"},
    {0x9114, "VIA1_T1L",  "VIA1 timer 1 low"},
    {0x9115, "VIA1_T1H",  "VIA1 timer 1 high"},
    {0x911E, "VIA1_IER",  "VIA1 interrupt enable"},
    {0x9120, "VIA2_PB",   "VIA2 port B (keyboard cols)"},
    {0x9121, "VIA2_PA",   "VIA2 port A (keyboard rows)"},
    {0x9124, "VIA2_T1L",  "VIA2 timer 1 low (60Hz IRQ)"},
    {0x9125, "VIA2_T1H",  "VIA2 timer 1 high (60Hz IRQ)"},
    {0x912E, "VIA2_IER",  "VIA2 interrupt enable"},
};

/* Common KERNAL jump-table entries a cartridge calls (high ROM, $E000-$FFFF). */
static const a2reg_t kernal[] = {
    {0xFF8A, "RESTOR",  "restore default I/O vectors"},
    {0xFF81, "CINT",    "init screen + VIC chip"},
    {0xFF84, "IOINIT",  "init I/O devices"},
    {0xFF87, "RAMTAS",  "RAM test, set pointers"},
    {0xFFD2, "CHROUT",  "output a character"},
    {0xFFCF, "CHRIN",   "input a character"},
    {0xFFE4, "GETIN",   "get a character"},
    {0xFFE1, "STOP",    "test RUN/STOP key"},
    {0xFFD5, "LOAD",    "load from device"},
    {0xFFBD, "SETNAM",  "set filename"},
    {0xFFBA, "SETLFS",  "set logical file"},
    {0xE55F, "CLRSCR",  "clear screen"},
    {0xE518, "SCRINIT", "screen/VIC init (CINT body)"},
    {0xFDF9, "IOINIT2", "I/O init body"},
    {0xFD8D, "RAMTAS2", "RAM test body"},
};

static const char *lookup(uint16_t a, int want_note) {
    for (size_t i = 0; i < sizeof(io_page)/sizeof(io_page[0]); i++)
        if (io_page[i].addr == a) return want_note ? io_page[i].note : io_page[i].name;
    for (size_t i = 0; i < sizeof(kernal)/sizeof(kernal[0]); i++)
        if (kernal[i].addr == a) return want_note ? kernal[i].note : kernal[i].name;
    return NULL;
}

const char *vic_reg_name(uint16_t addr) { return lookup(addr, 0); }
const char *vic_reg_note(uint16_t addr) { return lookup(addr, 1); }
