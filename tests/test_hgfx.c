/* Execute the production renderer with a packed 720x576 simulated video RAM. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/include/protocol.h"
#include "../src/spectrum/mbel/hgfx.h"

unsigned char test_registers[24], test_palette[HGFX_COLOURS*3], test_row[HGFX_ROW_BYTES];
static unsigned char frame[HGFX_HEIGHT][HGFX_ROW_BYTES], registers[256];
static unsigned int mapped_y;
static int locked;
padPt PLATOSize = {512,512};
CharMem CurMem;
padBool TTY, FlowControl, ModeBold, Rotate, Reverse;
DispMode CurMode;

void hgfx_map(unsigned int y) {
    assert(!locked && y < HGFX_HEIGHT);
    locked = 1; mapped_y = y;
    memcpy(test_row, frame[y], HGFX_ROW_BYTES);
}
void hgfx_unmap(void) {
    assert(locked);
    memcpy(frame[mapped_y], test_row, HGFX_ROW_BYTES);
    locked = 0;
}
void hgfx_reg(unsigned int value) { registers[value & 255] = value >> 8; }
unsigned int hgfx_getreg(unsigned int reg) { return registers[reg]; }
void hgfx_select_device(void) {}
void keyboard_main(void) { assert(!locked); }
void clg(void) {}
void zx_border(int colour) { (void)colour; }
void zx_hardcopy(void) {}
void bit_beep(int duration, int pitch) { (void)duration; (void)pitch; }

#include "../src/spectrum/mbel/hgfx.c"

static unsigned int count_pixels(unsigned char value)
{
    unsigned int count = 0;
    int x, y;
    for (y = 0; y < HGFX_CANVAS_HEIGHT; ++y) for (x = 0; x < 512; ++x) {
        unsigned int px = x + HGFX_X_ORIGIN;
        unsigned char v = frame[y+HGFX_Y_ORIGIN][px / (8/HGFX_BITS)];
        v = (v >> (8-HGFX_BITS-(px % (8/HGFX_BITS))*HGFX_BITS)) & (HGFX_COLOURS-1);
        if (v == value) ++count;
    }
    return count;
}

int main(void)
{
    padPt a = {0,0}, b = {511,511};
    padRGB red = {255,0,0}, green = {0,255,0}, blue = {0,0,255};
    unsigned char saved[HGFX_COLOURS*3], letter = 'A';
    unsigned int i, n;
    charData glyph;
    hgfx_start();
    assert(registers[1] == HGFX_MODE && registers[0x20] == (HGFX_MODE==5 ? 5 : 7));
    CurMode = ModeWrite;
    screen_dot_draw(&a); screen_dot_draw(&b);
    assert(get_pixel(0,0) == 1 && get_pixel(511,511) == 1);
#ifdef __HGFX3__
    assert(frame[383][0] == 0x10 && frame[0][255] == 1);
#else
    assert(frame[543][26] == 0x40 && frame[32][153] == 1);
#endif
    a.x = -1; screen_dot_draw(&a); a.x = 0;
    screen_clear();
    screen_line_draw(&a,&b);
    assert(count_pixels(1) == 512);
    for (i = 0; i < 512; ++i) assert(get_pixel(i,i) == 1);
    screen_clear(); a.x = 10; a.y = 20; b.x = 14; b.y = 24;
    screen_block_draw(&a,&b);
    assert(count_pixels(1) == 5*(scaled_y(20)-scaled_y(24)+1));
    CurMode = ModeErase; screen_block_draw(&b,&a);
    assert(count_pixels(1) == 0);
    CurMode = ModeRewrite; CurMem = M0;
    a.x = 8; a.y = 400;
    screen_char_draw(&a,&letter,1);
    n = 0;
    for (i = 0; i < 16; ++i) {
        unsigned char bits = h5_font[('A'-32)*16+i];
        unsigned int col;
#ifdef __HGFX3__
        if (i < 15 && scaled_y(415-i) == scaled_y(414-i)) continue;
#endif
        for (col = 0; col < 8; ++col) {
            unsigned char expected = !!(bits & (128 >> col));
            assert(get_pixel(8+col,415-i) == expected);
            n += expected;
        }
    }
    assert(count_pixels(1) == n);
    ModeBold = padT; Rotate = padT; Reverse = padT;
    screen_clear(); screen_char_draw(&a,&letter,1);
#ifndef __HGFX3__
    assert(count_pixels(1) == 4*n);
#else
    assert(count_pixels(1) > 0);
#endif
    ModeBold = Rotate = Reverse = padF;
    for (i = 0; i < 8; ++i) glyph[i] = 1U << (i*2);
    hgfx_load_char(0,glyph); letter = 32; CurMem = M2;
    screen_clear(); screen_char_draw(&a,&letter,1);
    assert(count_pixels(1) == 8);
    for (i = 0; i < 8; ++i) assert(get_pixel(8+i,400+i*2) == 1);
    hgfx_load_char(128,glyph); /* Out-of-range font index must not write. */
    screen_foreground(&red); assert(ink == 2);
    screen_foreground(&green); assert(ink == 3);
    for (i = 4; i < HGFX_COLOURS; ++i) {
        padRGB extra = {i*11, i*2, 17};
        screen_foreground(&extra);
        assert(ink == i);
    }
    memcpy(saved,test_palette,sizeof(saved));
    screen_foreground(&blue);
    assert(!memcmp(saved,test_palette,sizeof(saved))); /* No palette eviction/recolour. */
    assert(ink < HGFX_COLOURS);
    hgfx_suspend(); assert(registers[1] == 0);
    memset(test_palette,0,sizeof(saved)); hgfx_resume();
    assert(registers[1] == HGFX_MODE && !memcmp(saved,test_palette,sizeof(saved)));
    fg.red=fg.green=fg.blue=255;
    screen_clear(); enable_fill = 1;
    a.x=10; a.y=10; b.x=30; b.y=10; screen_line_draw(&a,&b);
    a=b; b.y=30; screen_line_draw(&a,&b);
    a=b; b.x=10; screen_line_draw(&a,&b);
    a=b; b.y=10; screen_line_draw(&a,&b);
    a.x=20; a.y=20; screen_paint(&a);
    assert(!hgfx_fill_overflow && count_pixels(1) == 21*(scaled_y(10)-scaled_y(30)+1));
    assert(get_pixel(9,20) == 0 && get_pixel(20,9) == 0);
    assert(!locked);
    puts("HGFX tests passed: packing, geometry, native font, bold/rotate, loaded glyphs, stable RGB palette, suspend/resume, fill.");
    return 0;
}
