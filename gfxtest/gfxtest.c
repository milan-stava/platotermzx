/* GFXTEST for the unified PLATOTerm (eZX / MB03+): mode menu, then the same drawing steps as
 * PLATOTerm, after each step the frames (T step frames) and a key. Not part of PLATOTerm. */
#include <string.h>
#include "protocol.h"
#include "screen.h"
#include "splash.h"
#include <spectrum.h>
#include <stdio.h>
#include <conio.h>

extern void screen_init(void);
extern void screen_clear(void);
extern void screen_line_draw(padPt *a,padPt *b);
extern void screen_dot_draw(padPt *p);
extern void screen_block_draw(padPt *a,padPt *b);
extern void screen_char_draw(padPt *c,unsigned char *t,unsigned char n);
extern void screen_paint(padPt *p);
extern void screen_foreground(padRGB *c);
extern void screen_background(padRGB *c);
extern void hgfx_set_fill(unsigned char e);
extern void hgfx_char_load(padWord n,charData d);
extern void terminal_init(void);
extern void ShowPLATO(padByte *buff,unsigned short count);
extern CharMem CurMem;
extern DispMode CurMode;
extern padBool ModeBold,Rotate;
#ifdef TEST_HELP
extern void hgfx_help_run(void);
#endif

/* stubs for terminal/keyboard/io parts not used by graphics */
unsigned char already_started=0;
void io_send_byte(unsigned char b){ (void)b; }
void keyboard_main(void){ }
void keyboard_click(void){ }
void screen_beep(void){ }
static unsigned char key_feed;
unsigned int standard_raw_key(void){ return 0; }
unsigned char standard_getkey(void){ return 13; }
void standard_input_reset(void){ }

unsigned char mk;
#ifdef HWTEST
/* Real hardware: show the frames of the previous step (50 Hz FRAMES) in the
 * bottom-left corner and wait for a key (any key, released afterwards). */
static unsigned int last_frames;
static unsigned char nbuf[20];
static unsigned char key_down(void)
{
  return (inp(0x00fe)&0x1f)!=0x1f;
}
static void mark(unsigned char n)
{
  unsigned int now=*(unsigned int *)23672, d=now-last_frames;
  padPt p; unsigned char i=19; DispMode m=CurMode; CharMem cm=CurMem; padBool bo=ModeBold,ro=Rotate;
  mk=n;
  if(n==1 || n==70 || n==100 || n==102 || n==104 || n==106 || n==110 || n==108){ last_frames=now; return; }
  nbuf[i]=0;
  do{ nbuf[--i]='0'+d%10; d/=10; }while(d && i>6);
  nbuf[--i]=' '; nbuf[--i]='0'+n%10; if(n>=10)nbuf[--i]='0'+(n/10)%10; if(n>=100)nbuf[--i]='0'+n/100;
  nbuf[--i]='T';
#ifdef TEST_HELP
  if(n==2) { /* FDMA wait calibration (0 = CPU held during FDMA) */
    extern unsigned char g_wk; unsigned char w=g_wk;
    nbuf[--i]=' ';
    do{ nbuf[--i]='0'+w%10; w/=10; }while(w);
    nbuf[--i]='W';
  }
#endif
  CurMode=ModeRewrite; CurMem=M0; ModeBold=padF; Rotate=padF;
  p.x=0;p.y=0; screen_char_draw(&p,nbuf+i,19-i);
  CurMode=m; CurMem=cm; ModeBold=bo; Rotate=ro;
  while(!key_down());
  while(key_down());
  last_frames=*(unsigned int *)23672;
}
#else
static void mark(unsigned char n)
{
  mk=n;
#asm
  ld a,(_mk)
  out (0xab),a
#endasm
}
#endif

static unsigned int seed=12345;
static unsigned int rnd(void){ seed=seed*25173+13849; return seed; }

static padPt a,b;
static padRGB c;
static unsigned char txt[64];

static void colour(unsigned char r,unsigned char g,unsigned char bl,unsigned char fg)
{
  c.red=r;c.green=g;c.blue=bl;
  if(fg)screen_foreground(&c);else screen_background(&c);
}

static void text(int x,int y,const char *s)
{
  a.x=x;a.y=y;
  strcpy((char *)txt,s);
  screen_char_draw(&a,txt,strlen(s));
}

extern unsigned char gfx_select(unsigned char mode);
void main(void)
{
  int i;
#ifdef HWTEST
  {
    extern unsigned char gfx_switch(unsigned char mode);
    int k;
    if(!gfx_select(0)){ for(;;); }
    screen_init(); clrscr();
    printf("GFXTEST PLATOTerm unified\n\n 1  512 x 384  16 colours\n 2  720 x 576   4 colours\n 3  256 x 192  standard ZX\n\nAfter each step: frames (T n f),\npress a key.\nSelect 1-3: ");
    do { k=getch(); } while(k<'1' || k>'3');
    printf("%c\n",k);
    if(k!='1' && !gfx_switch((unsigned char)(k-'1'))){ printf("Graphics module damaged.\n"); for(;;); }
  }
#else
  if(!gfx_select(TMODE)){ mark(250); for(;;); }
#endif
  mark(1);
  screen_init();
  terminal_init();
  mark(2);
  ShowPLATO((padByte *)splash,sizeof(splash));
  mark(3);
  /* 4: lines */
  CurMode=ModeWrite;
  for(i=0;i<60;++i){
    a.x=rnd()&511;a.y=rnd()&511;b.x=rnd()&511;b.y=rnd()&511;
    if(i%5==0)b.y=a.y; if(i%7==0)b.x=a.x;
    screen_line_draw(&a,&b);
  }
  mark(4);
  /* 5: text WRITE / ERASE / REWRITE M0 M1 */
  screen_clear();
  CurMem=M0;
  CurMode=ModeWrite; text(8,480,"WRITE ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 abcdefghijklmnopqrstu");
  CurMode=ModeRewrite; text(16,456,"REWRITE The quick brown fox jumps over the lazy dog!");
  CurMode=ModeWrite; text(20,430,"OFFSET x=20 not aligned");
  CurMode=ModeErase; text(16,456,"ERASE");
  CurMem=M1; CurMode=ModeWrite; text(8,400,"M1 0123456789:;<=>?@ABCDEFG");
  CurMem=M0; CurMode=ModeRewrite; text(500,300,"EDGE"); text(0,0,"BOTTOM ROW");
  text(8,496,"TOP ROW");
  mark(5);
  /* 6: blocks */
  screen_clear();
  for(i=0;i<25;++i){
    a.x=rnd()&511;a.y=rnd()&511;b.x=rnd()&511;b.y=rnd()&511;
    CurMode=(i&3)==3?ModeErase:ModeWrite;
    if(i==10){a.x=3;b.x=4;} if(i==11){a.x=8;b.x=15;} if(i==12){a.x=0;a.y=0;b.x=511;b.y=511;CurMode=ModeErase;}
    if(i==13){a.x=100;b.x=100;}
    screen_block_draw(&a,&b);
    colour(rnd(),rnd(),rnd(),1);
  }
  mark(6);
  /* 7: fill */
  screen_clear(); CurMode=ModeWrite;
  colour(255,255,255,1);
  a.x=50;a.y=50;b.x=450;b.y=60;screen_line_draw(&a,&b);
  a=b;b.x=470;b.y=440;screen_line_draw(&a,&b);
  a=b;b.x=60;b.y=430;screen_line_draw(&a,&b);
  a=b;b.x=50;b.y=50;screen_line_draw(&a,&b);
  a.x=150;a.y=150;b.x=350;b.y=350;screen_line_draw(&a,&b);
  a.x=150;a.y=350;b.x=350;b.y=150;screen_line_draw(&a,&b);
  a.x=200;a.y=200;b.x=300;b.y=300;screen_block_draw(&a,&b);
  for(i=0;i<40;++i){ a.x=100+(rnd()&255);a.y=100+(rnd()&255); screen_dot_draw(&a); }
  /* a comb: many pockets */
  for(i=0;i<20;++i){ a.x=80+i*16;a.y=380;b.x=a.x;b.y=420;screen_line_draw(&a,&b); }
  hgfx_set_fill(1);
  colour(0,200,0,1);
  a.x=100;a.y=100;
  mark(70);
  screen_paint(&a);
  mark(7);
  /* 8: dots */
  colour(255,220,0,1);
  for(i=0;i<200;++i){ a.x=rnd()&511;a.y=rnd()&511; screen_dot_draw(&a); }
  mark(8);
  /* 9: inverse + bold + rotate */
  screen_clear(); colour(255,255,255,1);
  CurMem=M0;
  CurMode=ModeInverse; text(16,480,"INVERSE TEXT");
  a.x=10;a.y=10;b.x=500;b.y=10;screen_line_draw(&a,&b);
  CurMode=ModeWrite; ModeBold=padT; text(16,400,"BOLD W"); CurMode=ModeRewrite; text(16,350,"BOLD RW");
  ModeBold=padF; Rotate=padT; CurMode=ModeWrite; text(300,200,"ROT");
  Rotate=padF;
  mark(9);
  /* 10: timing - one text line and one long diagonal */
  CurMode=ModeRewrite;
  mark(100); text(0,100,"0123456789012345678901234567890123456789012345678901234567890123"); mark(101);
  CurMode=ModeWrite;
  a.x=0;a.y=0;b.x=511;b.y=300;
  mark(102); screen_line_draw(&a,&b); mark(103);
  a.x=10;a.y=10;b.x=400;b.y=210;
  mark(104); screen_block_draw(&a,&b); mark(105);
  mark(106); screen_clear(); mark(107);
  CurMode=ModeWrite; text(8,8,"X"); mark(108); text(8,8,"X"); mark(109);
#ifdef TEST_HELP
  mark(110); hgfx_help_run(); mark(111);
#endif
#ifdef HWTEST
  for(;;);
#else
#asm
  di
  halt
#endasm
#endif
}
