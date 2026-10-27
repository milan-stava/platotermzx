#ifdef __HGFX__
#include <string.h>
#include <stdlib.h>
#ifndef HGFX_HOST_TEST
#include <spectrum.h>
#include <graphics.h>
#include <sound.h>
#else
void clg(void);
void zx_border(int colour);
void zx_hardcopy(void);
void bit_beep(int duration, int pitch);
#define INK_RED 2
#endif
#include "hgfx.h"
#include "../../include/screen.h"
#include "../../include/keyboard.h"
#include "vendor/plato_font8x16.h"

#ifdef HGFX_HOST_TEST
extern unsigned char test_registers[24], test_palette[HGFX_COLOURS*3], test_row[HGFX_ROW_BYTES];
#define REGS test_registers
#define PALETTE test_palette
#define PIXELS test_row
#elif defined(__HGFX3__)
#define REGS ((volatile unsigned char *)0x0000)
#define PALETTE ((volatile unsigned char *)0x2000)
#define PIXELS ((volatile unsigned char *)0x4000)
#else
#define REGS ((volatile unsigned char *)0x5800)
#define PALETTE ((volatile unsigned char *)0x5900)
#define PIXELS ((volatile unsigned char *)0)
#endif
#define X_ORIGIN HGFX_X_ORIGIN
#define Y_ORIGIN HGFX_Y_ORIGIN
#define SPAN_LIMIT 256

unsigned char hgfx_active;
unsigned char hgfx_fill_overflow;
unsigned char hgfx_fontm23[2048];
unsigned char CharWide = 8, CharHigh = 16;
padPt TTYLoc;
char enable_fill;
static padRGB fg = {255,255,255}, bg = {0,0,0};
static padRGB colours[HGFX_COLOURS];
static unsigned char used[HGFX_COLOURS], ink = 1, paper;
static unsigned int seeds_x[SPAN_LIMIT], seeds_y[SPAN_LIMIT];
static unsigned int seed_count;

#if defined(__HGFX3__) && !defined(HGFX_HOST_TEST)
/* The planar aperture is a write interface; reads see underlying ZX RAM.
 * Never use LDIR/memset here: copying a previous store back can read stale
 * bitmap data instead of 255. Volatile stores emit no destination reads. */
static void planar_fill(unsigned int offset, unsigned int count)
{
    volatile unsigned char *p = PIXELS + offset;
    while (count--) *p++ = 255;
}
#endif

static void configure(void)
{
    unsigned char i;
#ifdef __HGFX3__
    hgfx_reg(0x0520); /* SARAH/probe: linear, no ZX transparency */
#else
    hgfx_reg(0x0720); /* linear, no ZX transparency, enable register writes */
#endif
    hgfx_reg(0x4021); /* planar window stays away from code/stack */
#ifdef __HGFX3__
    hgfx_reg(0x0022); /* ROM writes: no ULA attribute/register collision */
    hgfx_reg(0x2023);
#else
    hgfx_reg(0x5822);
    hgfx_reg(0x5923);
#endif
#ifdef __HGFX3__
    hgfx_reg(0x0002); /* Exact BGR/video baseline verified by the probe. */
#else
    hgfx_reg((hgfx_getreg(2) & 0xf9) * 256 + 2); /* explicit BGR order */
#endif
    for (i = 0; i < 24; ++i) REGS[i] = 0;
#ifdef __HGFX3__
    REGS[4] = 15;
#else
    REGS[4] = 255;
#endif
}

static void palette_write(unsigned char slot)
{
    unsigned char offset = slot * 3;
    PALETTE[offset] = colours[slot].blue;
    PALETTE[offset+1] = colours[slot].green;
    PALETTE[offset+2] = colours[slot].red;
}

static unsigned char colour(padRGB *rgb)
{
    unsigned char i, best = 0;
    unsigned int distance, nearest = 65535;
    for (i = 0; i < HGFX_COLOURS; ++i)
        if (used[i] && !memcmp(rgb, &colours[i], sizeof(padRGB))) return i;
    for (i = 0; i < HGFX_COLOURS; ++i) if (!used[i]) {
        colours[i] = *rgb;
        used[i] = 1;
        palette_write(i);
        return i;
    }
    /* Never evict a palette entry still used by existing pixels. */
    for (i = 0; i < HGFX_COLOURS; ++i) {
        distance = abs((int)rgb->red - colours[i].red) +
                   abs((int)rgb->green - colours[i].green) +
                   abs((int)rgb->blue - colours[i].blue);
        if (distance < nearest) { nearest = distance; best = i; }
    }
    return best;
}

static void palette_reset(void)
{
    memset(used, 0, sizeof(used));
    colours[0] = bg;
    used[0] = 1;
    palette_write(0);
    paper = 0;
    ink = colour(&fg);
}

void hgfx_start(void)
{
    unsigned int i;
    /* Same device choice as the shared BIOS: external MB if detected. */
#ifndef __HGFX3__
    hgfx_select_device();
#endif
    configure();
    hgfx_reg((HGFX_MODE << 8) | 1);
    hgfx_active = 1;
    palette_reset();
    /* Clear side/top margins as well as the PLATO canvas. */
    for (i = 0; i < HGFX_HEIGHT; ++i) {
        hgfx_map(i);
#if defined(__HGFX3__) && !defined(HGFX_HOST_TEST)
        REGS[3] = 0;
        planar_fill(0, 64);
#else
        memset((void *)PIXELS, 0, HGFX_ROW_BYTES);
#endif
        hgfx_unmap();
    }
}

void hgfx_suspend(void)
{
    if (!hgfx_active) return;
#ifdef __HGFX3__
    hgfx_reg(0x8520);
#else
    hgfx_reg(0x8720);
#endif
    /* console writes must not modify HGFX regs/palette */
    hgfx_reg(0x0001);
}

void hgfx_resume(void)
{
    unsigned char i;
    if (!hgfx_active) return;
    configure();
    for (i = 0; i < HGFX_COLOURS; ++i) if (used[i]) palette_write(i);
    hgfx_reg((HGFX_MODE << 8) | 1);
}

/* Packed pixels, most significant group first. Caller maps one scanline. */
static unsigned char row_get(unsigned int x)
{
#ifdef __HGFX3__
#ifdef HGFX_HOST_TEST
    return (PIXELS[x >> 1] >> (4 - ((x & 1) << 2))) & 15;
#else
    return (hgfx_raw_read(x >> 1) >> (4 - ((x & 1) << 2))) & 15;
#endif
#else
    return (PIXELS[x >> 2] >> (6 - ((x & 3) << 1))) & 3;
#endif
}

static void row_set(unsigned int x, unsigned char value)
{
#if defined(__HGFX3__) && !defined(HGFX_HOST_TEST)
    /* INK replaces selected bitplanes for each set bit in the byte. */
    REGS[3] = value;
    PIXELS[x >> 3] = 128 >> (x & 7);
#else
#ifdef __HGFX3__
    unsigned char shift = 4 - ((x & 1) << 2);
    unsigned int address = x >> 1;
#else
    unsigned char shift = 6 - ((x & 3) << 1);
    unsigned int address = x >> 2;
#endif
    PIXELS[address] = (PIXELS[address] & ~((HGFX_COLOURS-1) << shift)) | (value << shift);
#endif
}

static unsigned int scaled_y(int y)
{
#ifdef __HGFX3__
    return ((511-y) * 3) >> 2;
#else
    return 511-y;
#endif
}

static unsigned char get_pixel(int x, int y)
{
    unsigned char value;
    hgfx_map(Y_ORIGIN + scaled_y(y));
    value = row_get(x + X_ORIGIN);
    hgfx_unmap();
    return value;
}

static void pixel(int x, int y, unsigned char value)
{
    if ((unsigned int)x > 511 || (unsigned int)y > 511) return;
    hgfx_map(Y_ORIGIN + scaled_y(y));
    row_set(x + X_ORIGIN, value);
    hgfx_unmap();
}

static void span(int x1, int x2, int y, unsigned char value)
{
#if defined(__HGFX3__) && !defined(HGFX_HOST_TEST)
    unsigned int first, last;
    unsigned char left, right;
#else
    int x;
#endif
    if ((unsigned int)y > 511 || x2 < 0 || x1 > 511) return;
    if (x1 < 0) x1 = 0;
    if (x2 > 511) x2 = 511;
    hgfx_map(Y_ORIGIN + scaled_y(y));
#if defined(__HGFX3__) && !defined(HGFX_HOST_TEST)
    REGS[3] = value;
    first = x1 >> 3;
    last = x2 >> 3;
    left = 255 >> (x1 & 7);
    right = 255 << (7 - (x2 & 7));
    if (first == last) PIXELS[first] = left & right;
    else {
        PIXELS[first] = left;
        if (last > first+1) planar_fill(first+1, last-first-1);
        PIXELS[last] = right;
    }
#else
    for (x = x1 + X_ORIGIN; x <= x2 + X_ORIGIN; ++x) row_set(x, value);
#endif
    hgfx_unmap();
}

void screen_init(void) { clg(); }
void screen_wait(void) {}
void screen_beep(void) { bit_beep(30, 500); }

void screen_clear(void)
{
    int y;
    palette_reset();
#if defined(__HGFX3__) && !defined(HGFX_HOST_TEST)
    /* Full physical rows, same proven INK path as hgfx_start/probe.
     * Per-pixel clearing costs hundreds of millions of cycles. */
    REGS[3] = paper;
    for (y = 0; y < HGFX_HEIGHT; ++y) {
        hgfx_map(y);
        planar_fill(0, 64);
        hgfx_unmap();
    }
#else
    for (y = 0; y < 512; ++y) span(0, 511, y, paper);
#endif
}

void screen_foreground(padRGB *rgb) { fg = *rgb; ink = colour(rgb); }
void screen_background(padRGB *rgb) { bg = *rgb; paper = colour(rgb); }

/* Keep these branches explicit: sccz80 2.4 miscompiles an OR of enum
 * comparisons used as the condition of a ternary expression at -O2.
 */
static unsigned char pen_colour(void)
{
    if (CurMode == ModeErase) return paper;
    if (CurMode == ModeInverse) return paper;
    return ink;
}

void screen_dot_draw(padPt *p)
{
    pixel(p->x, p->y, pen_colour());
}

void screen_block_draw(padPt *a, padPt *b)
{
    int x1 = a->x < b->x ? a->x : b->x;
    int x2 = a->x > b->x ? a->x : b->x;
    int y1 = a->y < b->y ? a->y : b->y;
    int y2 = a->y > b->y ? a->y : b->y;
    unsigned char value = pen_colour();
    int y;
    if (y1 < 0) y1 = 0;
    if (y2 > 511) y2 = 511;
    for (y = y1; y <= y2; ++y) span(x1, x2, y, value);
}

void screen_line_draw(padPt *a, padPt *b)
{
    int x = a->x, y = a->y;
    int dx = abs(b->x - x), dy = -abs(b->y - y);
    int sx = x < b->x ? 1 : -1, sy = y < b->y ? 1 : -1;
    int error = dx + dy, twice;
    unsigned char value = pen_colour();
    for (;;) {
        pixel(x, y, value);
        if (x == b->x && y == b->y) return;
        twice = error * 2;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
    }
}

void screen_char_draw(padPt *p, unsigned char *text, unsigned char count)
{
    int x = p->x, y = p->y, gx, gy, dx, dy;
    unsigned char n, row, col, bits, sx, sy, scale = ModeBold ? 2 : 1;
    unsigned char index, value, set;
    const unsigned char *font;
    for (n = 0; n < count; ++n) {
        if (CurMem == M0 || CurMem == M2) index = text[n] - 32;
        else index = text[n] + (CurMem == M1 ? 64 : 32);
        if (CurMem == M0 || CurMem == M1) {
            if (index >= 160) continue;
            font = h5_font + ((unsigned int)index << 4);
        } else {
            if (index >= 128) continue;
            font = hgfx_fontm23 + ((unsigned int)index << 4);
        }
        for (row = 0; row < 16; ++row) {
            bits = font[row];
            for (col = 0; col < 8; ++col) {
                set = bits & (128 >> col);
                if (!set && CurMode != ModeRewrite && CurMode != ModeInverse) continue;
                value = pen_colour();
                if (!set) {
                    value = paper;
                    if (CurMode == ModeInverse) value = ink;
                }
                for (sy = 0; sy < scale; ++sy) for (sx = 0; sx < scale; ++sx) {
                    dx = col * scale + sx;
                    dy = (15 - row) * scale + sy;
                    gx = x + (Rotate ? dy : dx);
                    gy = y + (Rotate ? dx : dy);
#ifdef __HGFX3__
                    /* Sample once per physical row. Write and rewrite modes
                     * must use the same glyph, rather than ORing aliased rows.
                     */
                    if (!Rotate && gy > y && scaled_y(gy) == scaled_y(gy-1)) continue;
                    if (Rotate && gy < y+8*scale-1 && scaled_y(gy) == scaled_y(gy+1)) continue;
#endif
                    pixel(gx, gy, value);
                }
            }
        }
        if (Rotate) y = (y + (Reverse ? -8 : 8) * scale) & 511;
        else x = (x + (Reverse ? -8 : 8) * scale) & 511;
        keyboard_main();
    }
}

void screen_tty_char(padByte ch)
{
    if (ch >= 32 && ch < 127) {
        screen_char_draw(&TTYLoc, &ch, 1);
        TTYLoc.x += CharWide;
    } else if (ch == 11) TTYLoc.y += CharHigh;
    else if (ch == 8 && TTYLoc.x > 7) TTYLoc.x -= CharWide;
    else if (ch == 10) TTYLoc.y -= CharHigh;
    else if (ch == 13) TTYLoc.x = 0;
    if (TTYLoc.x + CharWide > 511) { TTYLoc.x = 0; TTYLoc.y -= CharHigh; }
    if (TTYLoc.y < 0) { screen_clear(); TTYLoc.y = 495; }
}

void hgfx_load_char(padWord number, charData columns)
{
    unsigned char x, y;
    unsigned int word;
    unsigned char *glyph;
    if ((unsigned int)number >= 128) return;
    glyph = hgfx_fontm23 + ((unsigned int)number << 4);
    memset(glyph, 0, 16);
    for (x = 0; x < 8; ++x) {
        word = columns[x];
        for (y = 0; y < 16; ++y)
            if (word & (1U << y)) glyph[15-y] |= 128 >> x;
    }
}

static int seed(int x, int y)
{
    if (seed_count == SPAN_LIMIT) { hgfx_fill_overflow = 1; return 0; }
    seeds_x[seed_count] = x;
    seeds_y[seed_count++] = y;
    return 1;
}

void screen_paint(padPt *p)
{
    int x, y, left, right, scan, neighbour;
    unsigned char old, value, inside;
    if (!enable_fill || (unsigned int)p->x > 511 || (unsigned int)p->y > 511) return;
    value = pen_colour();
    old = get_pixel(p->x, p->y);
    if (old == value) return;
    seed_count = 0;
    hgfx_fill_overflow = 0;
    seed(p->x, scaled_y(p->y));
    while (seed_count) {
        --seed_count;
        x = seeds_x[seed_count]; y = seeds_y[seed_count];
        hgfx_map(Y_ORIGIN + y);
        if (row_get(X_ORIGIN + x) != old) { hgfx_unmap(); continue; }
        left = right = x;
        while (left && row_get(X_ORIGIN + left-1) == old) --left;
        while (right < 511 && row_get(X_ORIGIN + right+1) == old) ++right;
        for (x = left; x <= right; ++x) row_set(X_ORIGIN + x, value);
        hgfx_unmap();
        for (neighbour = -1; neighbour <= 1; neighbour += 2) {
            scan = y + neighbour;
            if ((unsigned int)scan >= HGFX_CANVAS_HEIGHT) continue;
            inside = 0;
            hgfx_map(Y_ORIGIN + scan);
            for (x = left; x <= right; ++x) {
                if (row_get(X_ORIGIN + x) == old) {
                    if (!inside && !seed(x, scan)) {
                        hgfx_unmap(); zx_border(INK_RED); return;
                    }
                    inside = 1;
                } else inside = 0;
            }
            hgfx_unmap();
        }
    }
}

void hgfx_hardcopy(void)
{
    unsigned int x, y, source_y, address;
    unsigned char bits, b, value;
    hgfx_suspend();
    memset((void *)0x4000, 0, 6144);
    memset((void *)0x5800, 7, 768);
    hgfx_resume();
    /* Printer's 256x192 raster samples the entire native canvas. */
    for (y = 0; y < 192; ++y) {
#ifdef __HGFX3__
        source_y = y * 2;
#else
        source_y = Y_ORIGIN + y * 8 / 3;
#endif
        address = 0x4000 | ((y & 192) << 5) | ((y & 7) << 8) | ((y & 56) << 2);
        for (x = 0; x < 32; ++x) {
            bits = 0;
            hgfx_map(source_y);
            for (b = 0; b < 8; ++b) {
                value = row_get(X_ORIGIN + (x * 8 + b) * 2);
                if (value != paper) bits |= 128 >> b;
            }
            hgfx_unmap();
            /* Disable planar writes while populating the ULA bitmap. */
            hgfx_reg(0x8720);
            *((unsigned char *)(address+x)) = bits;
#ifdef __HGFX3__
            hgfx_reg(0x0520);
#else
            hgfx_reg(0x0720);
#endif
        }
    }
    hgfx_suspend();
    zx_hardcopy();
    hgfx_resume();
}

void screen_done(void) { hgfx_suspend(); }

void hgfx_demo(void)
{
    static const unsigned char rgb[16][3] = {
        {0,0,0},{255,255,255},{255,0,0},{0,255,0},
        {0,0,255},{255,255,0},{0,255,255},{255,0,255},
        {128,128,128},{255,128,0},{128,0,255},{0,128,255},
        {128,255,0},{255,0,128},{0,255,128},{192,192,255}
    };
    padPt a = {0,0}, b = {511,511};
    padRGB c;
    unsigned char i;
    CurMode = ModeWrite; CurMem = M0;
    ModeBold = Rotate = Reverse = padF;
    fg.red = fg.green = fg.blue = 255;
    bg.red = bg.green = bg.blue = 0;
    hgfx_start();
    screen_line_draw(&a,&b);
    a.x=511; b.x=0; screen_line_draw(&a,&b);
    a.x=0; a.y=0; b.x=511; b.y=0; screen_line_draw(&a,&b);
    a.y=511; b.y=511; screen_line_draw(&a,&b);
    a.x=0; a.y=0; b.x=0; screen_line_draw(&a,&b);
    a.x=511; b.x=511; screen_line_draw(&a,&b);
    for (i=0; i<HGFX_COLOURS; ++i) {
        c.red=rgb[i][0]; c.green=rgb[i][1]; c.blue=rgb[i][2];
        screen_foreground(&c);
        a.x=8+i*(496/HGFX_COLOURS); a.y=240;
        b.x=a.x+(496/HGFX_COLOURS)-3; b.y=300;
        screen_block_draw(&a,&b);
    }
    c.red=c.green=c.blue=255; screen_foreground(&c);
    a.x=8; a.y=472;
#ifdef __HGFX3__
    screen_char_draw(&a,(unsigned char *)"HGFX 3 - 512x384 / 16 RGB colours",33);
#else
    screen_char_draw(&a,(unsigned char *)"HGFX 5 - 720x576 / 4 RGB colours",32);
#endif
    a.y=440;
    screen_char_draw(&a,(unsigned char *)"PLATO native font: ABCDEFG abcdefg 0123456789",45);
    a.y=24;
    screen_char_draw(&a,(unsigned char *)"Press any key to return to connection setup.",44);
}
#endif
