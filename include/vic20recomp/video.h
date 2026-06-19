/* video.h - Commodore VIC-20 display readout for recompiled code.
 *
 * The VIC (6560/6561) has no framebuffer: the picture is a fixed scan-out of RAM
 * whose geometry is set by the VIC registers ($9000-$900F). The readout below
 * reconstructs one frame the way the chip does:
 *
 *   - the video matrix (screen RAM, default $1E00) holds cols x rows character
 *     codes (default 22 x 23); base + bit9 come from $9005 hi nibble + $9002 b7.
 *   - each cell's 8x8 glyph is fetched from character memory ($9005 lo nibble;
 *     default $8000 CHARGEN ROM, or a custom charset in RAM).
 *   - the per-cell foreground colour is the low nibble of colour RAM ($9600);
 *     bit 3 selects multicolour mode for that cell.
 *   - $900F holds the screen background (hi nibble) and border (low 3 bits) plus
 *     the reverse-video bit; $900E hi nibble is the multicolour auxiliary colour.
 *
 * Like the Apple II hi-res scan and the Pac-Man tile grid, this is a readout
 * that reads RAM - no per-pixel program involvement. When no CHARGEN ROM is present
 * (bring-your-own) and a cell's glyph would come from the empty ROM region, a
 * built-in 8x8 font keyed by VIC screen code is substituted so text still shows.
 */
#ifndef VIC20RECOMP_VIDEO_H
#define VIC20RECOMP_VIDEO_H

#include <stdint.h>

/* Max scan-out (cols/rows capped at 32) plus a border on every side. */
#define VIC_BORDER   16
#define VIC_MAX_W    (32 * 8 + 2 * VIC_BORDER)
#define VIC_MAX_H    (32 * 8 + 2 * VIC_BORDER)

/* The 16-colour VIC-20 hardware palette, 0x00RRGGBB, in chip colour-index order
 * (0 black, 1 white, 2 red, 3 cyan, 4 purple, 5 green, 6 blue, 7 yellow, then
 * the eight lighter variants). */
extern const uint32_t vic_palette[16];

/* Render the live VIC frame from vic_ram into `out` (must hold VIC_MAX_W*VIC_MAX_H
 * 0xAARRGGBB pixels). Writes the actual rendered size (including border) to *w/*h.
 * No external dependencies. */
void vic_render_rgba(uint32_t *out, int *w, int *h);

/* Count non-background pixels in the current frame (a cheap "did anything draw"
 * metric for headless bring-up). */
long vic_lit_pixels(void);

#endif /* VIC20RECOMP_VIDEO_H */
