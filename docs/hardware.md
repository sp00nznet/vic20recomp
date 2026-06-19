# VIC-20 hardware notes (for the recompiler)

The Commodore VIC-20 (1980/81) is a MOS 6502 at ~1.02 MHz with a single custom
chip, the **VIC** (6560 NTSC / 6561 PAL), and two 6522 VIAs. Like the Apple II
hi-res scan and the Pac-Man tile grid, the picture is **a region of RAM the VIC
chip scans** — there is no framebuffer to reverse-engineer.

## Memory map (unexpanded)

| Range        | Use |
|--------------|-----|
| `$0000-$00FF`| zero page |
| `$0100-$01FF`| CPU stack |
| `$0200-$03FF`| KERNAL/BASIC work area; RAM IRQ vector at `$0314` |
| `$1000-$1FFF`| 4K user RAM (default screen `$1E00`, custom charsets live here too) |
| `$8000-$8FFF`| CHARGEN character ROM (bring-your-own) |
| `$9000-$900F`| VIC chip registers |
| `$9110-$912F`| VIA #1 / #2 (timers → 60 Hz IRQ, keyboard, joystick) |
| `$9400-$95FF`| colour RAM (expanded) — low nibble per cell |
| `$9600-$97FF`| colour RAM (unexpanded default) — low nibble per cell |
| `$A000-$BFFF`| cartridge ROM (autostart) |
| `$C000-$DFFF`| BASIC ROM (bring-your-own) |
| `$E000-$FFFF`| KERNAL ROM (bring-your-own) |

The runtime models all of this as **one flat 64K array** (`vic_ram`): cartridge
ROM, screen RAM, colour RAM and the VIC registers are all just addresses. A host
may intercept the `$90xx/$91xx` I/O page (`vic_io_hook`) to make raster/timer
reads progress; nothing else needs special handling for a static frame render.

## Cartridge autostart

An autostart cart maps at `$A000` with the signature **`A0CBM`** (`$41 $30 $C3
$C2 $CD`) at `$A004-$A008`. The **cold-start vector** is the word at `$A000`. In
TOSEC, a `[A000]` `.crt` is a 2-byte little-endian load-address header (`$00
$A0`) followed by the raw ROM (some dumps are header-less raw images). Boot is:
map the ROM at `$A000`, set `PC = read16($A000)`, run.

A clean cart (e.g. *Jelly Monsters*) pokes the VIC registers straight from a
table in its own ROM and copies a custom character set into RAM — so it needs no
KERNAL/CHARGEN to draw its screen. For carts that lean on the KERNAL (e.g.
*Gorf* calls `RAMTAS`/`RESTOR`/`IOINIT`/`CINT`), a host fills `$C000-$FFFF` with
`RTS` so those calls return, and pre-seeds the VIC register defaults `CINT`
would leave.

## VIC video registers (`$9000-$900F`)

| Reg | Meaning |
|-----|---------|
| `$9002` | bit 7 = screen address bit 9; bits 0-6 = number of columns (22) |
| `$9003` | bits 1-6 = number of rows (23); bit 0 = 8×8 / 8×16 chars |
| `$9004` | raster line (read) |
| `$9005` | bits 4-7 = video-matrix base nibble; bits 0-3 = character base nibble |
| `$900E` | bits 4-7 = multicolour auxiliary colour; bits 0-3 = volume |
| `$900F` | bits 4-7 = background; bit 3 = normal/reverse; bits 0-2 = border |

**Address translation** (the A13-inversion quirk): a 4-bit nibble `n` maps to CPU
address `n<8 ? $8000+n*$400 : (n-8)*$400`. So `$9005=$F0` → screen `$1C00` (+`$200`
from `$9002` bit 7 → `$1E00`), charset `$8000`; a custom RAM charset at `$1400`
is selected with char nibble `13` (`$D`). Colour RAM follows the screen bit 9:
`$9600` when `$9002` bit 7 is set, else `$9400`.

Each cell: 8×8 glyph from character memory, foreground colour from the colour-RAM
low 3 bits; bit 3 selects per-cell multicolour mode (2 bits/pixel: background,
border, cell colour, auxiliary). The 16-colour hardware palette is fixed.

When no CHARGEN ROM is present and a glyph would come from the empty `$8000`
region, the readout substitutes a built-in 8×8 font keyed by VIC screen code, so
text still appears. A custom RAM charset is rendered exactly as the chip sees it.
