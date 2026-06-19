/* video.c - Commodore VIC-20 display readout. See video.h. */
#include "vic20recomp/video.h"
#include "vic20recomp/mem.h"
#include <stddef.h>

/* VIC-20 16-colour hardware palette (0x00RRGGBB), chip colour-index order. */
const uint32_t vic_palette[16] = {
    0x000000, /* 0  black        */ 0xFFFFFF, /* 1  white        */
    0x772D26, /* 2  red          */ 0x85D4DC, /* 3  cyan         */
    0xA85FB4, /* 4  purple       */ 0x559E4A, /* 5  green        */
    0x42348B, /* 6  blue         */ 0xBDCC71, /* 7  yellow       */
    0xA8734A, /* 8  orange       */ 0xE9B287, /* 9  light orange */
    0xB66862, /* 10 light red    */ 0xC5FFFF, /* 11 light cyan   */
    0xE99DF5, /* 12 light purple */ 0x92DF87, /* 13 light green  */
    0x7E70CA, /* 14 light blue   */ 0xFFFFB0, /* 15 light yellow */
};

/* Built-in 8x8 font (public-domain font8x8_basic subset, printable ASCII
 * $20-$7F; LSB = leftmost pixel). Used only when no CHARGEN ROM is present and a
 * cell's glyph would come from the empty $8000 region. */
static const unsigned char VIC_FONT[96][8] = {
 {0,0,0,0,0,0,0,0},{24,24,24,24,24,0,24,0},{54,54,0,0,0,0,0,0},{54,54,127,54,127,54,54,0},
 {12,62,3,30,48,31,12,0},{0,99,51,24,12,102,99,0},{28,54,28,110,59,51,110,0},{6,6,3,0,0,0,0,0},
 {24,12,6,6,6,12,24,0},{6,12,24,24,24,12,6,0},{0,102,60,255,60,102,0,0},{0,12,12,63,12,12,0,0},
 {0,0,0,0,0,12,12,6},{0,0,0,63,0,0,0,0},{0,0,0,0,0,12,12,0},{96,48,24,12,6,3,1,0},
 {62,99,115,123,111,103,62,0},{12,14,12,12,12,12,63,0},{30,51,48,28,6,51,63,0},{30,51,48,28,48,51,30,0},
 {56,60,54,51,127,48,120,0},{63,3,31,48,48,51,30,0},{28,6,3,31,51,51,30,0},{63,51,48,24,12,12,12,0},
 {30,51,51,30,51,51,30,0},{30,51,51,62,48,24,14,0},{0,12,12,0,0,12,12,0},{0,12,12,0,0,12,12,6},
 {24,12,6,3,6,12,24,0},{0,0,63,0,0,63,0,0},{6,12,24,48,24,12,6,0},{30,51,48,24,12,0,12,0},
 {62,99,123,123,123,3,30,0},{12,30,51,51,63,51,51,0},{31,51,51,31,51,51,31,0},{30,51,3,3,3,51,30,0},
 {15,27,51,51,51,27,15,0},{63,3,3,31,3,3,63,0},{63,3,3,31,3,3,3,0},{30,51,3,59,51,51,62,0},
 {51,51,51,63,51,51,51,0},{30,12,12,12,12,12,30,0},{120,48,48,48,51,51,30,0},{51,27,15,7,15,27,51,0},
 {3,3,3,3,3,3,63,0},{51,63,127,127,99,99,99,0},{51,55,63,63,59,51,51,0},{30,51,51,51,51,51,30,0},
 {31,51,51,31,3,3,3,0},{30,51,51,51,59,30,56,0},{31,51,51,31,27,51,51,0},{30,51,3,30,48,51,30,0},
 {63,12,12,12,12,12,12,0},{51,51,51,51,51,51,30,0},{51,51,51,51,51,30,12,0},{99,99,99,107,127,63,54,0},
 {99,54,28,8,28,54,99,0},{51,51,51,30,12,12,12,0},{127,51,24,12,6,51,127,0},{30,6,6,6,6,6,30,0},
 {3,6,12,24,48,96,64,0},{30,24,24,24,24,24,30,0},{8,28,54,99,0,0,0,0},{0,0,0,0,0,0,0,255},
 {6,12,0,0,0,0,0,0},{0,0,30,48,62,51,110,0},{3,3,31,51,51,51,30,0},{0,0,30,3,3,3,30,0},
 {48,48,62,51,51,51,110,0},{0,0,30,51,63,3,30,0},{28,54,6,15,6,6,15,0},{0,0,110,51,51,62,48,31},
 {3,3,31,55,51,51,51,0},{12,0,14,12,12,12,30,0},{48,0,56,48,48,51,51,30},{3,3,51,27,15,27,51,0},
 {14,12,12,12,12,12,30,0},{0,0,53,127,107,99,99,0},{0,0,31,51,51,51,51,0},{0,0,30,51,51,51,30,0},
 {0,0,31,51,51,31,3,3},{0,0,62,51,51,62,48,120},{0,0,59,110,6,6,6,0},{0,0,62,3,30,48,31,0},
 {8,12,62,12,12,44,24,0},{0,0,51,51,51,51,110,0},{0,0,51,51,51,30,12,0},{0,0,99,107,127,63,54,0},
 {0,0,99,54,28,54,99,0},{0,0,51,51,51,62,48,31},{0,0,63,25,12,38,63,0},{56,12,12,7,12,12,56,0},
 {24,24,24,0,24,24,24,0},{7,12,12,56,12,12,7,0},{110,59,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},
};

/* VIC nibble -> CPU address (the A13-inversion quirk: 0-7 map to $8000-$9C00,
 * 8-15 map to $0000-$1C00). */
static uint16_t nib_addr(int nib) {
    return (uint16_t)(nib >= 8 ? (nib - 8) * 0x0400 : 0x8000 + nib * 0x0400);
}

static int chargen_present(void) {
    for (int i = 0x8000; i < 0x9000; i++) if (vic_ram[i]) return 1;
    return 0;
}

/* VIC screen code -> built-in font glyph (printable ASCII $20-$7F window). */
static const unsigned char *builtin_glyph(uint8_t code) {
    uint8_t c = code & 0x7F;             /* drop reverse bit; handled by caller */
    int ascii = (c < 32) ? c + 64 : c;   /* 0-31 -> @,A-Z,[\]^_ ; 32-63 -> punct/digits */
    if (ascii < 0x20 || ascii > 0x7F) ascii = 0x20;
    return VIC_FONT[ascii - 0x20];
}

typedef struct { int cols, rows; uint16_t screen, charbase, color;
                 int bg, border, aux, normal, builtin; } vic_disp_t;

static void vic_describe(vic_disp_t *d) {
    uint8_t r2 = vic_ram[0x9002], r3 = vic_ram[0x9003], r5 = vic_ram[0x9005];
    uint8_t rE = vic_ram[0x900E], rF = vic_ram[0x900F];
    d->cols = r2 & 0x7F; if (d->cols < 1 || d->cols > 32) d->cols = 22;
    d->rows = (r3 >> 1) & 0x3F; if (d->rows < 1 || d->rows > 32) d->rows = 23;
    d->screen   = (uint16_t)(nib_addr((r5 >> 4) & 0x0F) + ((r2 & 0x80) ? 0x0200 : 0));
    d->charbase = nib_addr(r5 & 0x0F);
    d->color    = (r2 & 0x80) ? 0x9600 : 0x9400;
    d->bg     = (rF >> 4) & 0x0F;
    d->border = rF & 0x07;
    d->normal = (rF >> 3) & 1;            /* 1 = normal, 0 = whole-screen reverse */
    d->aux    = (rE >> 4) & 0x0F;
    /* Built-in font only when glyphs would come from an absent CHARGEN ROM. */
    d->builtin = (d->charbase >= 0x8000 && d->charbase < 0x9000) && !chargen_present();
}

void vic_render_rgba(uint32_t *out, int *w, int *h) {
    vic_disp_t d; vic_describe(&d);
    int W = d.cols * 8 + 2 * VIC_BORDER, H = d.rows * 8 + 2 * VIC_BORDER;
    uint32_t bgc = 0xFF000000u | vic_palette[d.bg];
    uint32_t bdc = 0xFF000000u | vic_palette[d.border];
    for (int i = 0; i < W * H; i++) out[i] = bdc;

    for (int cy = 0; cy < d.rows; cy++) {
        for (int cx = 0; cx < d.cols; cx++) {
            uint16_t cell = (uint16_t)(d.screen + cy * d.cols + cx);
            uint8_t code = vic_ram[cell];
            uint8_t ccol = vic_ram[(uint16_t)(d.color + cy * d.cols + cx)];
            int fg = ccol & 0x07;
            int multi = (ccol & 0x08) && !d.builtin;
            const unsigned char *g = d.builtin ? builtin_glyph(code) : NULL;
            int px0 = VIC_BORDER + cx * 8, py0 = VIC_BORDER + cy * 8;
            for (int ry = 0; ry < 8; ry++) {
                uint8_t byte = g ? g[ry] : vic_ram[(uint16_t)(d.charbase + code * 8 + ry)];
                uint32_t *row = out + (size_t)(py0 + ry) * W + px0;
                if (multi) {
                    for (int p = 0; p < 4; p++) {     /* 00=bg 01=border 10=fg 11=aux */
                        int bits = (byte >> (6 - 2 * p)) & 0x03;
                        int ci = bits == 0 ? d.bg : bits == 1 ? d.border :
                                 bits == 2 ? fg : d.aux;
                        uint32_t c = 0xFF000000u | vic_palette[ci];
                        row[p * 2] = c; row[p * 2 + 1] = c;
                    }
                } else {
                    for (int b = 0; b < 8; b++) {
                        int on = g ? (byte >> b) & 1 : (byte >> (7 - b)) & 1;
                        if (!d.normal) on = !on;
                        row[b] = on ? (0xFF000000u | vic_palette[fg]) : bgc;
                    }
                }
            }
        }
    }
    if (w) *w = W; if (h) *h = H;
}

long vic_lit_pixels(void) {
    vic_disp_t d; vic_describe(&d);
    long n = 0;
    for (int cy = 0; cy < d.rows; cy++)
        for (int cx = 0; cx < d.cols; cx++) {
            uint16_t cell = (uint16_t)(d.screen + cy * d.cols + cx);
            uint8_t code = vic_ram[cell];
            const unsigned char *g = d.builtin ? builtin_glyph(code) : NULL;
            for (int ry = 0; ry < 8; ry++) {
                uint8_t byte = g ? g[ry] : vic_ram[(uint16_t)(d.charbase + code * 8 + ry)];
                for (int b = 0; b < 8; b++) if ((byte >> b) & 1) n++;
            }
        }
    return n;
}
