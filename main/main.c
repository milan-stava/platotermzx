#include <stdio.h>
#include <conio.h>
#include "protocol.h"
#include "screen.h"
#include "terminal.h"
#include "io.h"
#include "keyboard.h"
#include "../standard/keyboard-input.h"
#include <spectrum.h>
extern unsigned char con_ink,con_paper;
extern unsigned char gfx_select(unsigned char mode);
extern unsigned char gfx_switch(unsigned char mode);
extern unsigned char gfx_mode,gfx_win;
extern unsigned char ovl_how;
extern unsigned char con_tty,logo_busy;
extern void con_replay(void);
extern void con_clear_text(void);
extern void con_clear_from(unsigned char row) __z88dk_fastcall;
unsigned char already_started=0;
static const unsigned char display_page[3]={14,13,15};

/* Start and Wi-Fi errors: the logo (splash) in the current display, text below it. */
/* The splash (1282 B) is not in the main memory: the loader keeps it in URAM page 16; it is
 * fetched by FDMA 24 into the user character area (5E00h, free before the server) and decoded. */
#define SPLASH_LEN 1282
extern unsigned char splash_load(void);
extern void gfx_reset(void);
static void logo(void)
{
  unsigned char ok,b=logo_busy;
  logo_busy=1;                          /* its screen_clear does not end the console phase */
  ok=splash_load();
  gfx_reset();                          /* FDMA 24 copy switched FDMA off: full set-up again */
  terminal_init();
  if(ok) ShowPLATO((padByte *)0x5e00,SPLASH_LEN);
  logo_busy=b;
}
/* Wi-Fi error (io.c): the logo again, the error text below it (console, server text too). */
void hgfx_console(void)
{
  con_ink=7;con_paper=0;
  if(gfx_mode==3)gfx_switch(0);         /* display 4: logo and text in 512x384, the window again
                                           when PLATO starts (gfx_win stays) */
  logo();
  con_clear_text();
  con_tty=1;
}

/* Display choice (io_init, before "Filled-in graphics?"): keys 1-4, the complete selected
 * label is inverse. 1 = ZX 256x192, 2 = HGFX 512x384, 3 = PAL 720x576 (start display, v9w),
 * 4 = HGFX 512x384 with the PLATO page 1:1 in a window (v9n): setup in 512x384, the window
 * display from the start of PLATO (gfx.c con_tty_end).
 * Another display: its module, the logo again and the text below it again. */
static const unsigned char key_mode[4]={2,0,1,0};     /* gfx.c: 0 HGFX 3, 1 PAL, 2 ZX */
extern unsigned char con_x,con_y;
extern void con_char_at(unsigned char x,unsigned char y,unsigned char c,unsigned char draw);
extern void con_text_at(unsigned char x,unsigned char y,const char *s,unsigned char inv);
static const char *display_label[4]={
  "1 ZX 256x192 (2 col)","2 HGFX 512x384 (16 col)",
  "3 PAL 720x576 (4 col)","4 HGFX 1:1 (16 col)"
};
static unsigned char choice_x[4],choice_y[4],choice_c,choice_ready;
static unsigned char fill_x[2],fill_y,fill_yes=1;

/* Store a complete option as normal/inverse.  Selection changes are then
 * replayed onto a clean screen, so the last glyph cannot be duplicated. */
static void display_mark(unsigned char index,unsigned char inv,unsigned char draw)
{
  const char *p=display_label[index];
  unsigned char x=choice_x[index],y=choice_y[index];
  if(draw)con_text_at(x,y,p,inv);
  else while(*p)con_char_at(x++,y,*p++,inv?3:0);
}
static void fill_mark(unsigned char yes,unsigned char inv,unsigned char draw)
{
  const char *p=yes?"YES":"NO";
  unsigned char x=fill_x[yes?0:1];
  if(draw)con_text_at(x,fill_y,p,inv);
  else while(*p)con_char_at(x++,fill_y,*p++,inv?3:0);
}
static void choices_replay(void)
{
  logo();
  con_replay();
}

/* display_apply (v9za): key '1'-'4' -> the display; the complete selected label is inverse.
 * Used by display_choice and again (any number of times) while "Filled-in graphics?" waits
 * (Milan, setup-control.asm setup_fill_getch). Another display: its module, the logo and the
 * text below it again; the same display (2 <-> 4): only the number. */
static unsigned char display_apply(int c)
{
  unsigned char index=c-'1',mode=key_mode[index];
  gfx_win=(c=='4');
  if(choice_ready) {
    display_mark(choice_c-'1',0,0);
    display_mark(index,1,0);
  }
  choice_c=c;
  logo_busy=1;
  if(mode!=gfx_mode) {
    if(!gfx_switch(mode)) {
      /* the module is damaged: back to the start display (720 x 576) */
      gfx_switch(1);
      gfx_win=0;
      if(choice_ready) {
        display_mark(index,0,0);
        choice_c='3';
        display_mark(2,1,0);
      }
      logo();
      con_replay();
      display_mark(2,1,1);
      fill_mark(fill_yes,1,1);
      printf("\nGraphics module %c damaged (URAM %u).",c,(unsigned)display_page[mode]);
      logo_busy=0;
      return 0;
    }
  }
  choices_replay();
  /* con_replay can leave one copy of the final inverse glyph.  Run the
   * in-place cleaner after replay for the two selected labels only. */
  display_mark(index,1,1);
  fill_mark(fill_yes,1,1);
  logo_busy=0;
  return 1;
}
unsigned char display_choice(void)
{
  int c;
  printf("Press 1-4: ");
  choice_x[0]=con_x;choice_y[0]=con_y;printf("%s  ",display_label[0]);
  choice_x[1]=con_x;choice_y[1]=con_y;printf("%s\n",display_label[1]);
  printf("            ");
  choice_x[2]=con_x;choice_y[2]=con_y;
  printf("%s",display_label[2]);
  choice_x[3]=con_x;choice_y[3]=con_y;printf("%s",display_label[3]);
  printf("\nFilled-in graphics: ");
  fill_x[0]=con_x;fill_y=con_y;
  printf("YES / ");
  fill_x[1]=con_x;printf("NO  [Y/N, ENTER]");
  /* Select PAL and fill directly in place; no second logo replay. */
  choice_c='3';choice_ready=1;
  fill_yes=1;
  display_mark(2,1,1);
  fill_mark(1,1,1);
  for(;;) {
    c=getch();
    if(c>='1' && c<='4') display_apply(c);
    else if(c=='y' || c=='Y') {
      if(!fill_yes) { fill_mark(0,0,1);fill_mark(1,1,1);fill_yes=1; }
    } else if(c=='n' || c=='N') {
      if(fill_yes) { fill_mark(1,0,1);fill_mark(0,1,1);fill_yes=0; }
    } else if(c==13 || c==10) break;
  }
  printf("\n");
  return fill_yes;
}

extern void basic_save(void);
extern void snd_start(void);
void main(void) {
  unsigned char m;
  basic_save();                         /* BASIC area 5B00h-5DFFh -> URAM 26 (EXTEND+X, v9q) */
  __asm
    ei
  __endasm;
  /* Start as the ZX version: the logo first (720 x 576 PAL, v9w), the setup below it. If the
   * module is damaged the next one is tried; the text then says which one. */
  for(m=0;m<3;++m)if(gfx_select(m==0?1:(m==1?0:2)))break;
  if(m==3) { zx_border(2); for(;;) ; }
  logo();
  if(m) {
    printf("Graphics module 3 damaged (URAM 13).\nLoad PLATOTerm again.\n");
    for(;;) ;
  }
  /* setup text and then the server's TTY text below the logo, row after row (console) until
   * the server switches to PLATO */
  con_clear_text();
  con_tty=1;
  standard_input_reset();
  io_init();
  terminal_initial_position();
  snd_start();                          /* 440 Hz as bit_play("A2"), any CPU speed (snd.asm) */
  for(;;) {
    keyboard_main();
    io_main();
    keyboard_service_local();
  }
}
