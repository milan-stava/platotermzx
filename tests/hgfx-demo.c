/* Standalone hardware test: no ESP, AP or server required. */
#ifdef __HGFX_DEMO__
#include <input.h>
#include "../src/include/screen.h"
#include "../src/include/terminal.h"

unsigned char already_started=0;
void io_send_byte(unsigned char c) { }
void io_main(void) { }
void io_init(void) { }

void main(void)
{
  padPt a,b;
  padRGB colour;
  unsigned char i;
  charData custom={0x8001,0x4002,0x2004,0x1008,0x0810,0x0420,0x0240,0x0180};
  screen_init();
  terminal_init();
  CurMode=ModeWrite;
  for (i=0; i<16; ++i) {
    colour.blue=(i&1)?((i&8)?255:192):0;
    colour.green=(i&4)?((i&8)?255:192):0;
    colour.red=(i&2)?((i&8)?255:192):0;
    if (i==8) colour.red=colour.green=colour.blue=96;
    screen_foreground(&colour);
    a.x=i*32; a.y=400; b.x=i*32+31; b.y=511;
    screen_block_draw(&a,&b);
  }
  colour.red=colour.green=colour.blue=255;
  screen_foreground(&colour);
  a.x=0; a.y=0; b.x=511; b.y=0; screen_line_draw(&a,&b);
  b.y=511; screen_line_draw(&a,&b);
  a.x=511; b.x=0; screen_line_draw(&a,&b);
  a.x=0; a.y=511; screen_line_draw(&a,&b);
  a.x=16; a.y=360;
  screen_char_draw(&a,(unsigned char *)"HGFX 512x384 - 16 COLOURS",23);
  a.y=328; CurMode=ModeRewrite;
  screen_char_draw(&a,(unsigned char *)"PLATO logical screen: 512x512",28);
  a.y=296; ModeBold=padT;
  screen_char_draw(&a,(unsigned char *)"BOLD",4);
  ModeBold=padF; Rotate=padT; a.x=480; a.y=200;
  screen_char_draw(&a,(unsigned char *)"ROTATED",7);
  Rotate=padF;
  terminal_char_load(0,custom);
  CurMem=M2; a.x=240; a.y=240;
  screen_char_draw(&a,(unsigned char *)" ",1);
  for (;;) in_Pause(100);
}
#endif
