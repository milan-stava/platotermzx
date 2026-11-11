/* eLeMeNt ZX / MB03+ HGFX 3 (mode 5), 512x384, 16 indexed colours.
 * Register reference: https://wiki.ilnx.cz/doku.php/hgfx:porty_registry
 * A single planar byte is a drawing aperture. Destination offsets place it
 * anywhere in the hardware buffer; no 96 KB framebuffer lives in Z80 RAM.
 */
#ifdef __HGFX__
#ifndef HGFX_TEST
#include <spectrum.h>
#include <sound.h>
#endif
#include <stdlib.h>
#include "../include/screen.h"
#include "../include/keyboard.h"

#ifdef HGFX_TEST
static unsigned char test_registers[32], test_palette[48];
static unsigned int test_offset_calls;
#define REG test_registers
#define PALETTE test_palette
extern void hgfx_test_write(int x,int y,unsigned char mask,unsigned char colour);
#else
#define REG ((volatile unsigned char *)0x5b00)
#define VIDEO ((volatile unsigned char *)0x4000)
#define PALETTE ((volatile unsigned char *)0x5800)
#endif

extern unsigned char font[];
extern unsigned short fontptr[];
unsigned char hgfx_user_font[128][16];
static unsigned char foreground=15, background=0, active=0;
static const unsigned char levels[2]={192,255};
static unsigned char palette[48];

static void setting(unsigned char index, unsigned char value)
{
#ifndef HGFX_TEST
  outp(0x7c3b,index);
  outp(0x7d3b,value);
#endif
}

static void write_pixels_at(unsigned char byte_index, unsigned char mask)
{
#ifdef HGFX_TEST
  hgfx_test_write(REG[5]+256U*REG[6]+byte_index*8,REG[7]+256U*REG[8],mask,REG[3]);
#else
  VIDEO[byte_index]=mask;
#endif
}

static void offset(int x, int y)
{
#ifdef HGFX_TEST
  ++test_offset_calls;
#endif
  REG[5]=x; REG[6]=x>>8;
  REG[7]=y; REG[8]=y>>8;
}

void hgfx_console(void)
{
  /* Disable interception as well as display: conio uses the ULA bitmap. */
  setting(0x20,0x80);
  setting(1,0);
  active=0;
}

void hgfx_resume(void)
{
  unsigned char i;
  if (active) return;
  setting(0x21,0x40);
  setting(0x22,0x5b);
  setting(0x23,0x58);
  setting(0x20,5); /* linear aperture, ULA transparency disabled */
  setting(1,5);
  REG[0]=0; REG[1]=0; REG[4]=15;
  REG[9]=0; REG[10]=0; REG[11]=0; REG[12]=0;
  REG[13]=0; REG[14]=background; REG[15]=15;
  for (i=0; i<48; ++i) PALETTE[i]=palette[i];
  active=1;
}

static int screen_y(int y) { return ((511-y)*3)/4; }

static void pixel(int x, int y, unsigned char colour)
{
  if (x<0 || x>=512 || y<0 || y>=384) return;
  hgfx_resume();
  REG[3]=colour;
  offset(x,y);
  write_pixels_at(0,0x80); /* zero bits must preserve surrounding pixels */
}

static void span(int x1, int x2, int y, unsigned char colour)
{
  int n, remaining;
  unsigned char byte_index;
  if (y<0 || y>=384 || x2<0 || x1>=512) return;
  if (x1<0) x1=0;
  if (x2>511) x2=511;
  hgfx_resume();
  REG[3]=colour;
  while (x1<=x2) {
    remaining=x2-x1+1;
    /* Stay within the first 32 bytes of the planar aperture, a single
     * 256-pixel row in either the original or wider memory layout. */
    if (remaining>256) remaining=256;
    offset(x1,y);
    byte_index=0;
    while (remaining) {
      n=remaining;
      if (n>8) n=8;
      write_pixels_at(byte_index++,(unsigned char)(0xff << (8-n)));
      x1+=n;
      remaining-=n;
    }
  }
}

void screen_init(void)
{
  unsigned char i, intensity;
  hgfx_resume();
  /* Spectrum colour order, with distinct dark grey at index 8. B,G,R. */
  for (i=0; i<16; ++i) {
    intensity=levels[i>>3];
    palette[i*3]=(i&1)?intensity:0;
    palette[i*3+1]=(i&4)?intensity:0;
    palette[i*3+2]=(i&2)?intensity:0;
  }
  palette[24]=palette[25]=palette[26]=96;
  for (i=0; i<48; ++i) PALETTE[i]=palette[i];
  screen_clear();
}

void screen_clear(void)
{
  int y;
  hgfx_resume();
  for (y=0; y<384; ++y) span(0,511,y,background);
}

void screen_beep(void) {
#ifndef HGFX_TEST
  bit_frequency(.2,440);
#endif
}
void screen_wait(void) { }

void hgfx_char_load(padWord charnum, charData data)
{
  unsigned char row, column;
  if (charnum<0 || charnum>=128) return;
  for (row=0; row<16; ++row) {
    hgfx_user_font[charnum][row]=0;
    for (column=0; column<8; ++column)
      if ((unsigned short)data[column] & (1U << (15-row)))
        hgfx_user_font[charnum][row] |= 0x80 >> column;
  }
}

static unsigned char drawing_colour(void)
{
  /* Keep the tests separate: some sccz80 versions miscompile an OR directly
   * in the condition of a byte-valued ternary expression. */
  if (CurMode==ModeErase) return background;
  if (CurMode==ModeInverse) return background;
  return foreground;
}

void screen_dot_draw(padPt *p)
{
  if (p->x<0 || p->x>511 || p->y<0 || p->y>511) return;
  pixel(p->x,screen_y(p->y),drawing_colour());
}

void screen_line_draw(padPt *a, padPt *b)
{
  int x=a->x, y=screen_y(a->y), x2=b->x, y2=screen_y(b->y);
  int dx=abs(x2-x), dy=-abs(y2-y), sx=(x<x2)?1:-1;
  int sy=(y<y2)?1:-1, error=dx+dy, twice;
  unsigned char colour=drawing_colour();
  /* Protocol coordinates are 9-bit. Reject corrupt endpoints before loops. */
  if (a->x<0 || a->x>511 || b->x<0 || b->x>511 ||
      a->y<0 || a->y>511 || b->y<0 || b->y>511) return;
  for (;;) {
    pixel(x,y,colour);
    if (x==x2 && y==y2) break;
    twice=error*2;
    if (twice>=dy) { error+=dy; x+=sx; }
    if (twice<=dx) { error+=dx; y+=sy; }
  }
}

void screen_block_draw(padPt *a, padPt *b)
{
  int x1=a->x, x2=b->x, y1=screen_y(a->y), y2=screen_y(b->y), t;
  if (x1>x2) { t=x1; x1=x2; x2=t; }
  if (y1>y2) { t=y1; y1=y2; y2=t; }
  if (y1<0) y1=0;
  if (y2>383) y2=383;
  for (; y1<=y2; ++y1) span(x1,x2,y1,drawing_colour());
}

void screen_char_draw(padPt *coord, unsigned char *text, unsigned char count)
{
  unsigned char c, row, col, bits, set, shift, bold=ModeBold?2:1;
  unsigned char *glyph;
  int origin_x=coord->x, origin_y=coord->y, x, y, u, v;
  while (count--) {
    c=*text++;
    if (CurMem==M0 || CurMem==M1) {
      c=(unsigned char)(c+((CurMem==M0)?-32:64));
      if (c>=160) continue;
      glyph=&font[fontptr[c]];
    } else {
      c=(unsigned char)(c+((CurMem==M2)?-32:32));
      if (c>=128) continue;
      glyph=hgfx_user_font[c];
    }
    for (row=0; row<16; ++row) {
      if (CurMem==M0 || CurMem==M1) {
        /* Existing 4x6 ROM font expanded into the logical 8x16 cell. */
        bits=glyph[(unsigned int)row*6/16];
      } else bits=glyph[row];
      if (!Rotate && bold==1 && origin_x>=0 && origin_x<=504) {
        y=origin_y+15-row;
        if (y<0 || y>511) continue;
        /* Choose the last source row where 16 logical rows map to 12. */
        if (row<15 && y>0 && screen_y(y)==screen_y(y-1)) continue;
        if (CurMem==M0 || CurMem==M1) {
          set=0;
          if (bits&0x80) set|=0xc0;
          if (bits&0x40) set|=0x30;
          if (bits&0x20) set|=0x0c;
          if (bits&0x10) set|=0x03;
          bits=set;
        }
        hgfx_resume();
        offset(origin_x,screen_y(y));
        REG[3]=drawing_colour();
        if (bits) write_pixels_at(0,bits);
        if (CurMode==ModeRewrite || CurMode==ModeInverse) {
          REG[3]=(CurMode==ModeInverse)?foreground:background;
          write_pixels_at(0,(unsigned char)~bits);
        }
        continue;
      }
      for (col=0; col<8; ++col) {
        shift=col;
        if (CurMem==M0) shift=col/2;
        if (CurMem==M1) shift=col/2;
        set=bits & (0x80 >> shift);
        if (!set && CurMode!=ModeRewrite && CurMode!=ModeInverse) continue;
        for (v=0; v<bold; ++v) for (u=0; u<bold; ++u) {
          x=col*bold+u; y=(15-row)*bold+v;
          if (Rotate) { int t=x; x=-y; y=t; }
          x+=origin_x; y+=origin_y;
          if (y>=0 && y<512)
            pixel(x,screen_y(y),set?drawing_colour():
                  ((CurMode==ModeInverse)?foreground:background));
        }
      }
    }
    if (Rotate) origin_y+=8*bold; else origin_x+=8*bold;
    keyboard_main();
  }
}

static unsigned char nearest(padRGB *c)
{
  unsigned char i, result=0;
  long best=2147483647L, distance, r,g,b;
  for (i=0; i<16; ++i) {
    /* Palette lives in RAM as well as HGFX hardware. */
    b=(int)c->blue-palette[i*3];
    g=(int)c->green-palette[i*3+1];
    r=(int)c->red-palette[i*3+2];
    distance=r*r+g*g+b*b;
    if (distance<best) { best=distance; result=i; }
  }
  return result;
}
void screen_foreground(padRGB *c) { foreground=nearest(c); }
void screen_background(padRGB *c) { background=nearest(c); }
#endif
