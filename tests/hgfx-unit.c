/* Executed as real Z80 code by z88dk-ticks; mock only the HGFX device. */
#include <stdio.h>
#include <stdlib.h>
#define __HGFX__
#define HGFX_TEST
#include "../src/spectrum/hgfx.c"

unsigned char font[960];
unsigned short fontptr[160];
CharMem CurMem=M0;
DispMode CurMode=ModeWrite;
padBool ModeBold=padF, Rotate=padF;
static long pixels;
static int query_x,query_y,query_colour,invalid;
static unsigned char colours[16];
void keyboard_main(void) { }

void hgfx_test_write(int x,int y,unsigned char mask,unsigned char colour)
{
  unsigned char i;
  for (i=0; i<8; ++i) if (mask & (0x80>>i)) {
    ++pixels;
    if (x+i<0 || x+i>=512 || y<0 || y>=384 || colour>15) invalid=1;
    if (x+i==query_x && y==query_y) query_colour=colour;
    colours[colour]=1;
  }
}

static void check(int condition, char *message)
{
  if (!condition) { printf("FAIL: %s\n",message); exit(1); }
}
static void reset(int x,int y)
{
  pixels=0; invalid=0; query_x=x; query_y=y; query_colour=-1;
}
int main(void)
{
  padPt a,b;
  padRGB c;
  charData native={0x8000,0,0,0,0,0,0,1};
  unsigned char i;
  reset(511,383);
  screen_init();
  check(pixels==196608L && query_colour==0 && !invalid,"clear covers 512x384");
  check(test_offset_calls==768,"clear sets offset twice per row, not per byte");
  check(screen_y(0)==383 && screen_y(511)==0,"PLATO Y endpoints");
  for (i=0; i<16; ++i) {
    c.blue=palette[i*3]; c.green=palette[i*3+1]; c.red=palette[i*3+2];
    check(nearest(&c)==i,"all 16 palette entries reachable");
  }
  a.x=511; a.y=0;
  reset(511,383); screen_dot_draw(&a);
  check(pixels==1 && query_colour==15 && !invalid,"bottom right dot");
  a.x=0; a.y=511; b.x=511; b.y=511;
  reset(511,0); screen_line_draw(&a,&b);
  check(pixels==512 && query_colour==15 && !invalid,"line keeps 9-bit X");
  a.x=511; a.y=0; b.x=0; b.y=511;
  reset(0,0); screen_line_draw(&a,&b);
  check(pixels==512 && query_colour==15 && !invalid,"reverse diagonal endpoints");
  a.x=20; a.y=511; b.x=10; b.y=507;
  reset(20,3); screen_block_draw(&a,&b);
  check(pixels==44 && query_colour==15 && !invalid,"reversed inclusive block and short masks");
  CurMode=ModeErase;
  reset(10,0); screen_block_draw(&a,&b);
  check(query_colour==0,"erase uses background");
  a.x=-1; a.y=0;
  reset(0,0); screen_dot_draw(&a);
  check(pixels==0,"out of range coordinate rejected");
  a.x=508; a.y=0; b.x=520; b.y=511;
  reset(511,383); screen_block_draw(&a,&b);
  check(pixels==1536 && !invalid,"block clips right edge");
  CurMem=M2; CurMode=ModeRewrite;
  hgfx_char_load(0,native);
  check(hgfx_user_font[0][0]==0x80 && hgfx_user_font[0][15]==1 &&
        hgfx_user_font[0][7]==0,"native 8x16 character transposition");
  a.x=504; a.y=0;
  test_offset_calls=0;
  reset(511,383); screen_char_draw(&a,(unsigned char *)" ",1);
  check(query_colour==15 && !invalid,"custom glyph bottom row and right edge");
  check(test_offset_calls==12,"normal glyph uses 12 row writes, not 128 pixels");
  CurMode=ModeInverse;
  reset(511,383); screen_char_draw(&a,(unsigned char *)" ",1);
  check(query_colour==0,"inverse glyph foreground becomes background");
  reset(510,383); screen_char_draw(&a,(unsigned char *)" ",1);
  check(query_colour==15,"inverse glyph blank becomes foreground");
  ModeBold=padT; Rotate=padT; a.x=0; a.y=500;
  reset(0,0); screen_char_draw(&a,(unsigned char *)" ",1);
  check(pixels>0 && !invalid,"rotated bold glyph clips edges");
  hgfx_console(); hgfx_resume();
  check(PALETTE[47]==255 && REG[4]==15,"console restores palette and mask");
  puts("PASS: HGFX geometry, clipping, 16 colours, erase, custom text, bold/rotation, console");
  return 0;
}
