#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise the real renderer with a bitmapped pixel sink, without ZX ROM. */
#define __GFX_H__
static unsigned char pixels[6144];
static unsigned char before[6144];
void plot(int x,int y);
void unplot(int x,int y);
void draw(int x,int y,int x2,int y2);
void undraw(int x,int y,int x2,int y2);
void clg(void) { memset(pixels,0,sizeof(pixels)); }
void fill(int x,int y) { }
#include "../standard/screen.c"
unsigned short scalex[512],scaley[512],fontptr[160];
unsigned char font[960],fontm23[768],FONT_SIZE_X=4,FONT_SIZE_Y=6;
unsigned char standard_enable_fill=1;
padBool FastText,TTY,ModeBold,Rotate,Reverse;
CharMem CurMem=M0;
DispMode CurMode;
static void check(int ok) { if(!ok){puts("FAIL: renderer pixels/coordinates");exit(1);} }
void plot(int x,int y) {
  check(x>=0 && x<256 && y>=0 && y<192);
  pixels[y*32+x/8]|=128>>(x&7);
}
void unplot(int x,int y) {
  check(x>=0 && x<256 && y>=0 && y<192);
  pixels[y*32+x/8]&=~(128>>(x&7));
}
void draw(int x,int y,int x2,int y2) {
  check(y==y2); for(;x<=x2;++x)plot(x,y);
}
void undraw(int x,int y,int x2,int y2) {
  check(y==y2); for(;x<=x2;++x)unplot(x,y);
}
int main(void) {
  int i,x,y,order,mode,on;
  padPt a,b,where;
  unsigned char text='A';
  for(i=0;i<512;++i){scalex[i]=i/2;scaley[i]=191-(i*3/8);}
  for(i=0;i<160;++i)fontptr[i]=i*6;
  for(mode=0;mode<4;++mode)for(order=0;order<4;++order) {
    CurMode=mode;
    a.x=(order&1)?80:40; b.x=(order&1)?40:80;
    a.y=(order&2)?160:144; b.y=(order&2)?144:160;
    memset(pixels,0xa5,sizeof(pixels));
    screen_block_draw(&a,&b);
    for(y=0;y<192;++y)for(x=0;x<256;++x) {
      on=0;
      if(x>=20 && x<=40 && y>=131 && y<=137) {
        if(mode==ModeWrite || mode==ModeRewrite) on=1;
      } else {
        if(0xa5 & (128>>(x&7))) on=1;
      }
      if(((pixels[y*32+x/8] & (128>>(x&7)))!=0)!=on) {
        printf("mode=%d order=%d x=%d y=%d expected=%d byte=%u\n",mode,order,x,y,on,pixels[y*32+x/8]);
        check(0);
      }
    }
  }
  /* Rewrite text must not mutate caller coordinates or double-advance TTY. */
  memset(font,0xf0,sizeof(font));
  CurMode=ModeRewrite; FastText=padF;
  where.x=40;where.y=144;
  screen_char_draw(&where,&text,1);
  check(where.x==40 && where.y==144);
  TTYLoc=where;screen_tty_char('A');
  check(TTYLoc.x==48 && TTYLoc.y==144);
  /* Actual IRATA login coordinates captured with public guest input.
   * NEXT explicitly erases 24,223..39,208, i.e. the active-field arrow. */
  clg(); CurMode=ModeWrite; FastText=padT;
  where.x=24;where.y=208;screen_char_draw(&where,&text,1);
  where.x=40;screen_char_draw(&where,&text,1);
  where.x=24;where.y=224;screen_char_draw(&where,&text,1);
  memcpy(before,pixels,sizeof(pixels));
  a.x=24;a.y=223;b.x=39;b.y=208;CurMode=ModeInverse;
  screen_block_draw(&a,&b);
  for(y=0;y<192;++y)for(x=0;x<256;++x) {
    on=0;
    if(x<12 || x>19 || y<108 || y>113)
      if(before[y*32+x/8] & (128>>(x&7))) on=1;
    check(((pixels[y*32+x/8] & (128>>(x&7)))!=0)==on);
  }
  puts("PASS: actual IRATA NEXT erase removes arrow only, prompt and input preserved");
  puts("PASS: rectangle edges, four modes/corner orders, surrounding pixels, text coordinates");
  return 0;
}
