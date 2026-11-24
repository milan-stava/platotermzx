#include <conio.h>
#include "protocol.h"
#include "screen.h"
unsigned char CharWide=8,CharHigh=16;
padPt TTYLoc;
/* Until the server switches to PLATO (con_tty, gfx.c / main.c) its TTY text goes to the console
 * below the logo: on the next rows after the setup text, scrolling below the logo. */
extern unsigned char con_tty,con_tty_text;
void screen_tty_char(padByte c) {
  if(con_tty) {
    if(c>=32 && c<127) { putch(c); con_tty_text=1; }
    else if(c==10) putch('\n');
    return;
  }
  if(c>=32 && c<127) { screen_char_draw(&TTYLoc,&c,1);TTYLoc.x+=8; }
  else if(c==8 && TTYLoc.x>=8) TTYLoc.x-=8;
  else if(c==10) TTYLoc.y-=16;
  else if(c==11) TTYLoc.y+=16;
  else if(c==13) TTYLoc.x=0;
  if(TTYLoc.x+8>511) { TTYLoc.x=0;TTYLoc.y-=16; }
  if(TTYLoc.y<0) { screen_clear();TTYLoc.y=495; }
}
void screen_done(void) { }
void screen_clear_status_row(void) {
  padPt a={0,0},b={511,15};
  DispMode saved=CurMode;
  CurMode=ModeErase;screen_block_draw(&a,&b);CurMode=saved;
}
