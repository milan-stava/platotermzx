/* PLATOTerm unified (eZX / MB03+): mode selection and the non-hot C part of the graphics.
 *
 * Two graphics modules (overlays, gfx/ovl/ovl_*.asm), both loaded from the TAP into URAM at
 * start; the chosen one is copied into the window E800h-FFFFh:
 *   0  HGFX 3  (05h) 512x384, 16 colours        URAM page 14
 *   1  HGFX 5  (07h) 720x576, 4 colours FullPAL  URAM page 13
 *   2  ZX standard 256x192 (ULA, HGFX off)        URAM page 15 - as the original PLATOTerm ZX
 *   3  HGFX 3  (05h) 512x384, PLATO 1:1 window      URAM page 17 - display 4 (v9n): 384 of the 512
 *      rows, CS+3 / CS+4 move the window; the page is replayed from its record (URAM 18-25)
 * The text console (setup, Wi-Fi) is drawn below the logo in the chosen graphics (con.asm,
 * con_draw here), as in the original ZX version; the start display is 720x576 PAL (v9w).
 * This file keeps what is called rarely (mode on / off, palettes, HELP, the slow text path,
 * user characters, PAINT entry); drawing is in the module.
 *
 * Memory: window 4000h-57FFh, palette 5800h-5AFFh, HGFX registers 5B00h, FDMA 5D00h-5D3Fh,
 * user font M2/M3 5E00h-65FFh, BUF 6600h, T2 6700h, CPU stack 6800h-6FFFh (CLEAR 28671),
 * program 7000h-, module E800h-FFFFh. URAM 0-11 HELP copy (PAL 0-7), 12 PAINT stack,
 * 13-15 modules.
 */
#include <spectrum.h>
#include <string.h>
#include "protocol.h"
#include "screen.h"
#include "keyboard.h"

extern unsigned int standard_raw_key(void);
extern unsigned char standard_getkey(void);
extern void standard_input_reset(void);
extern void outp(unsigned int port,unsigned char value);
extern unsigned char inp(unsigned int port);

#define REG ((volatile unsigned char *)0x5b00)
#define PALETTE ((volatile unsigned char *)0x5800)
#define FDMA_REG ((volatile unsigned char *)0x5d00)
#define user_font ((unsigned char (*)[16])0x5e00)

extern unsigned char hgfx_native_font[];
extern unsigned char con_ink,con_paper;
extern padBool FastText;

/* Graphics module (gfxmain.asm -> jump table of the overlay). */
extern int g_x1,g_y1,g_x2,g_y2;
extern unsigned char g_col,g_paper,g_opaque,g_bold,g_cnt,g_mem;
extern unsigned char *g_txt;
extern void gfx_init(void);
extern void gfx_pixel(void);
extern void gfx_rect(void);
extern void gfx_clear(void);
extern void gfx_fill(void);
extern unsigned char gfx_save(void);
extern void gfx_restore(void);
extern int gfx_trx(int x) __z88dk_fastcall;
extern int gfx_try(int y) __z88dk_fastcall;
extern unsigned char ovl_load(unsigned char page) __z88dk_fastcall;
extern void hgfx_transpose(unsigned char *source,unsigned char *destination);

#define M_HGFX3 0
#define M_PAL   1
#define M_ZX    2
#define M_HGW   3
static const unsigned char mode_reg[4]={5,7,0,5};       /* ZX: g_zxi_001 = 0, HGFX off */
static const unsigned char mode_set[4]={5,7,0x80,5};     /* g_zxi_020 */
static const unsigned char mode_page[4]={14,13,15,17};
static const int mode_w[4]={512,720,256,512};
static const int mode_h[4]={384,576,192,384};
unsigned char gfx_mode;
/* 0 = the module setink must set the HGFX registers again (mode_on clears them) */
unsigned char g_inkok;
/* ZX module: changes with every loaded user character (its 4x6 copies are made again) */
unsigned char g_cgen;

/* Shared with the module. */
unsigned char gfx_fg=15,gfx_bg=0,gfx_paper=0,gfx_active=0,gfx_local;
#define foreground gfx_fg
#define background gfx_bg
#define paper gfx_paper
#define active gfx_active
#define local_render gfx_local
unsigned char hgfx_help_restored;
static unsigned char fill_enabled=1;
static unsigned char fdma_ready;

/* Page palette (v9m): the colours of the current page, as the host sent them - FullPAL 4,
 * HGFX 3 16 entries. After each clear: 0 = background, 1 = foreground, every new colour takes
 * the next free entry with its exact RGB; when all are used, the nearest one is taken. */
unsigned char foreground_rgb[3]={255,255,255};   /* also the window module (replay) */
unsigned char background_rgb[3]={0,0,0};
static unsigned char page_palette[16][3];
static unsigned int palette_used;
#define PAGE_N ((gfx_mode==M_PAL)?4:16)

static void setting(unsigned char index,unsigned char value)
{
  outp(0x7c3b,index);
  outp(0x7d3b,value);
}

static void pal_palette_set(unsigned char index,unsigned char red,unsigned char green,unsigned char blue)
{
  unsigned int offset=(unsigned int)(index&15)*3;
  PALETTE[offset]=blue;PALETTE[offset+1]=green;PALETTE[offset+2]=red;
}

/* Mode on: g_zxi_001 first (MB03+ resets 021-023 when it is written), then the areas,
 * settings, all registers and the palette (both ignored before the mode is on). */
static void mode_on(unsigned char reg,unsigned char set)
{
  unsigned char i;
  setting(0x40,0);fdma_ready=0;
  setting(1,reg);
  setting(0x21,0x40);
  setting(0x22,0x5b);
  setting(0x23,0x58);
  setting(0x20,set);
  setting(2,0);
  *(volatile unsigned char *)0x5c48 &= 0xc7; /* BORDCR: black paper/border */
  zx_border(0);
  for(i=0;i<32;++i)REG[i]=0;
  g_inkok=0;
}

/* Text console below the logo (con.asm): 24 rows over the screen as the original ZX console
 * (row r = screen rows 8r..8r+7 of 192): bottom PLATO y = (23 - r) * 64 / 3, so row 12 starts
 * just below the logo (its last rule is at y 268); 64 columns, drawn by the module (its font,
 * Rewrite; a light paper from textbackground = Inverse).
 * Rows 12-23 are also kept as text (63 columns, char | 80h = inverse) in the user character area
 * 6304h-65F7h (free until the server loads characters; 5E00h-6303h takes the logo data): the
 * area scrolls up at its end and is drawn again after a change of the display.
 * con_tty: until the server switches to PLATO, its TTY text goes to this console as well, so it
 * goes on below the setup lines (tty.c). */
unsigned char con_c,con_x,con_y,con_tty,con_tty_text,logo_busy;
extern void con_pause(void);
static const int con_row_y[24]={490,469,448,426,405,384,362,341,320,298,277,256,234,213,192,170,149,128,106,85,64,42,21,0};
#define CON_BUF ((unsigned char *)0x6304)
#define CON_W 63

static void con_put(unsigned char c,unsigned char x,unsigned char y,unsigned char inv)
{
  padPt p;
  DispMode m=CurMode;CharMem cm=CurMem;padBool b=ModeBold,r=Rotate,f=FastText;
  unsigned char l=local_render;
  p.x=(int)x*8;p.y=con_row_y[y];
  CurMode=inv?ModeInverse:ModeRewrite;CurMem=M0;ModeBold=padF;Rotate=padF;FastText=padF;
  local_render=1;
  screen_char_draw(&p,&c,1);
  local_render=l;
  CurMode=m;CurMem=cm;ModeBold=b;Rotate=r;FastText=f;
}

void con_draw(void)
{
  unsigned char inv=con_paper?1:0;
  if(con_y>=12 && con_y<24 && con_x<CON_W) CON_BUF[(con_y-12)*CON_W+con_x]=con_c|(inv?0x80:0);
  con_put(con_c,con_x,con_y,inv);
}

/* v9za: character c at console column x, row y (12..23): text buffer, drawn when draw != 0
 * (the number of the display choice, main.c display_apply). */
void con_char_at(unsigned char x,unsigned char y,unsigned char c,unsigned char draw)
{
  unsigned char inv=(draw>1);
  if(y<12 || y>23 || x>=CON_W)return;
  CON_BUF[(y-12)*CON_W+x]=c|(inv?0x80:0);
  /* draw 0/3 only update the replay buffer (normal/inverse).  The display
   * selector uses this before a clean logo + console replay, avoiding an
   * inverse glyph being painted over an existing normal glyph. */
  if(draw==1 || draw==2)con_put(c,x,y,inv);
}

static void con_cells_erase(unsigned char x,unsigned char y,unsigned char n)
{
  padPt a,b;
  DispMode m=CurMode;
  unsigned char l=local_render;
  a.x=(int)x*8;a.y=con_row_y[y];
  b.x=(int)(x+n)*8-1;b.y=a.y+15;
  CurMode=ModeErase;local_render=1;
  screen_block_draw(&a,&b);
  local_render=l;CurMode=m;
}

/* Replace one short console label in place.  The extra normal (non-inverse)
 * space is part of the redraw: it overwrites a stale final glyph directly.
 * This is more reliable in every display module than erasing that cell after
 * the inverse text has already been drawn. */
void con_text_at(unsigned char x,unsigned char y,const char *s,unsigned char inv)
{
  unsigned char n=0;
  const char *p=s;
  if(y<12 || y>23 || x>=CON_W)return;
  while(*p++ && x+n<CON_W)++n;
  con_cells_erase(x,y,n+(x+n<CON_W?1:0));
  while(*s && x<CON_W)con_char_at(x++,y,*s++,inv?2:1);
  if(x<CON_W)con_char_at(x,y,' ',1);
}

static void con_erase(unsigned char row)
{
  padPt a,b;
  DispMode m=CurMode;
  unsigned char l=local_render;
  a.x=0;a.y=0;b.x=511;b.y=row?con_row_y[row]+20:511;
  CurMode=ModeErase;
  local_render=1;
  screen_block_draw(&a,&b);
  local_render=l;
  CurMode=m;
}

/* Rows row..23 cleared (row 0 = the whole screen), also their text. */
void con_clear_from(unsigned char row) __z88dk_fastcall
{
  unsigned char r=row<12?12:row;
  con_erase(row);
  memset(CON_BUF+(r-12)*CON_W,0,(24-r)*CON_W);
}

/* Text of rows 12-23 forgotten (start, logo drawn again after an error). */
void con_clear_text(void)
{
  memset(CON_BUF,0,12*CON_W);
}

/* Rows 12-23 (text kept above) drawn again: after a change of the display and when scrolling. */
void con_replay(void)
{
  unsigned char x,y,c,*q=CON_BUF;
  for(y=12;y<24;++y)for(x=0;x<CON_W;++x,++q)if((c=*q) && c!=' ')con_put(c&0x7f,x,y,c>>7);  /* plain space = the erased paper */
}

/* ULA address of character row r, pixel line l. */
static unsigned char *con_zxl(unsigned char r,unsigned char l)
{
  return (unsigned char *)(0x4000+((unsigned)(r&0x18)<<8)+((unsigned)l<<8)+((r&7)<<5));
}

/* Past row 23: the text below the logo moves up by one row (the logo stays). */
void con_scroll(void)
{
  memmove(CON_BUF,CON_BUF+CON_W,11*CON_W);
  memset(CON_BUF+11*CON_W,0,CON_W);
  if(gfx_mode==M_ZX) {
    /* ZX: console row r = character row r of the ULA screen - the bytes move up (fast) */
    unsigned char r,l;
    for(r=12;r<23;++r)for(l=0;l<8;++l)memcpy(con_zxl(r,l),con_zxl(r+1,l),32);
    for(l=0;l<8;++l)memset(con_zxl(23,l),0,32);
    return;
  }
  con_erase(12);
  con_replay();
}

/* The server switched to PLATO (screen_clear outside the logo): console text area free again for
 * user characters - zeroed, the ZX module makes its reduced copies again. */
unsigned char gfx_win;                  /* display 4 chosen (main.c display_choice) */
extern void cpu_fast(void);
extern void cpu_restore(void);
unsigned char gfx_switch(unsigned char mode);
static void con_tty_end(void)
{
  /* the server's TTY text (tty.c) stays readable for about 2 s before PLATO clears the screen */
  if(con_tty_text){con_tty_text=0;con_pause();}
  con_tty=0;
  memset((unsigned char *)0x5e00,0,2048);
  if(++g_cgen==0xff)g_cgen=0;
  /* Keep the CPU frequency selected by the user/system.  Older builds forced
   * eLeMeNt ZX to 20 MHz here; FDMA calibration already supports the current
   * frequency and does not require changing it. */
  active=0;
  /* display 4: setup and server text were in 512x384 (whole logo); PLATO pages in the window */
  if(gfx_win && gfx_mode!=M_HGW && !gfx_switch(M_HGW)){gfx_win=0;gfx_switch(M_HGFX3);}
}

/* Graphics on (again) in the selected mode, registers and palette complete. */
void hgfx_resume(void)
{
  unsigned char i;
  if(active)return;
  mode_on(mode_reg[gfx_mode],mode_set[gfx_mode]);
  if(gfx_mode==M_PAL) {
    REG[4]=3;
    for(i=0;i<4;++i)pal_palette_set(i,page_palette[i][0],page_palette[i][1],page_palette[i][2]);
  } else if(gfx_mode!=M_ZX) {   /* ZX: 5800h are the ULA attributes (module clears them) */
    REG[4]=15;REG[14]=background;REG[15]=15;
    for(i=0;i<16;++i)pal_palette_set(i,page_palette[i][0],page_palette[i][1],page_palette[i][2]);
  }
  active=1;
  /* FDINIT order: off, register page, clear all 64 registers, on. */
  setting(0x40,0);
  setting(0x41,0x5d);
  for(i=0;i<64;++i)FDMA_REG[i]=0;
  setting(0x40,1);
  gfx_init();
  fdma_ready=1;
}

/* EXTEND+X (keyboard.c, then exit): graphics off, BASIC area 5B00h-5DFFh back (v9q). */
extern void basic_restore(void);
void hgfx_shutdown(void)
{
  setting(0x40,0);fdma_ready=0;
  setting(0x20,0x80);setting(1,0);active=0;
  cpu_restore();                        /* CPU speed as before (v9t) */
  basic_restore();
}

/* Mode choice at start: the module from URAM into the window. 0 = module missing. */
unsigned char gfx_select(unsigned char mode)
{
  gfx_mode=mode;
  if(mode!=M_ZX){gfx_fg=1;gfx_bg=0;}
  if(mode==M_ZX){gfx_fg=7;gfx_bg=0;}
  return ovl_load(mode_page[mode]);
}

void gfx_reset(void);
/* Change of the display after the start (display choice): module, mode on, page cleared. */
unsigned char gfx_switch(unsigned char mode)
{
  if(!gfx_select(mode))return 0;
  gfx_reset();
  return 1;
}
/* Graphics and FDMA set up again completely (after an FDMA 24 copy, which switches FDMA off). */
void gfx_reset(void)
{
  active=0;
  screen_init();
}

void hgfx_set_fill(unsigned char enabled)
{
  fill_enabled=enabled?1:0;
  /* PLATO PAINT is a seed fill over chunky memory, not the wired HGFX FILL. */
  if(active && gfx_mode!=M_PAL){REG[14]=background;REG[15]=15;REG[13]=0;}
}

/* Screen coordinates, clipped. */
static void rect(int x1,int y1,int x2,int y2,unsigned char colour)
{
  int t;
  if(x1>x2){t=x1;x1=x2;x2=t;}
  if(y1>y2){t=y1;y1=y2;y2=t;}
  if(x1<0)x1=0;
  if(x2>=mode_w[gfx_mode])x2=mode_w[gfx_mode]-1;
  if(y1<0)y1=0;
  if(y2>=mode_h[gfx_mode])y2=mode_h[gfx_mode]-1;
  if(x1>x2 || y1>y2)return;
  hgfx_resume();
  g_x1=x1;g_y1=y1;g_x2=x2;g_y2=y2;g_col=colour;
  gfx_rect();
}

/* FullPAL has no ULA border: two thin bands in the unused PAL margin show EXTEND.
 * HGFX 3 has no free margin: nothing. */
void hgfx_extend_indicator(unsigned char on)
{
  unsigned char colour;
  if(gfx_mode!=M_PAL)return;
  colour=on?foreground:background;
  rect(0,0,719,3,colour);
  rect(0,572,719,575,colour);
}

static unsigned char same_rgb(const unsigned char *a,const unsigned char *b)
{
  return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];
}

static void install_colour(unsigned char index,const unsigned char *rgb)
{
  page_palette[index][0]=rgb[0];page_palette[index][1]=rgb[1];page_palette[index][2]=rgb[2];
  pal_palette_set(index,rgb[0],rgb[1],rgb[2]);
  palette_used|=1u<<index;
}

static long distance(const unsigned char *a,const unsigned char *rgb)
{
  long dr,dg,db;
  dr=(int)rgb[0]-a[0];dg=(int)rgb[1]-a[1];db=(int)rgb[2]-a[2];
  return dr*dr+dg*dg+db*db;
}

/* brightness = the strongest of R, G, B (pure blue counts as bright: it is visible on black) */
static unsigned char lum(const unsigned char *c)
{
  unsigned char m=c[0];
  if(c[1]>m)m=c[1];
  if(c[2]>m)m=c[2];
  return m;
}
#define LUM_DARK 48                     /* an entry darker than this is "black" */
#define LUM_LIGHT 96                    /* an ink at least this bright never becomes "black" */
/* ink = 1 (foreground): when the palette is full and the colour has to be approximated, the
 * nearest entry other than the paper (page paper and the current background) is taken, so the
 * ink stays visible (v9o). v9z: a light ink is not approximated by a black entry either - the
 * NeXT cube of IRATA (PAL, 4 colours: blue paper, white, black cube, red N) drew its X in black
 * on the black cube. Only when no other entry is left, the black one is taken. */
static unsigned char page_colour(const unsigned char *rgb,unsigned char ink)
{
  unsigned char i,best_i=0,pass,light;
  long best,d;
  unsigned char n=PAGE_N;
  for(i=0;i<n;++i)if((palette_used&(1u<<i)) && same_rgb(page_palette[i],rgb))return i;
  for(i=0;i<n;++i)if(!(palette_used&(1u<<i))){install_colour(i,rgb);return i;}
  light=(ink && lum(rgb)>=LUM_LIGHT);
  for(pass=light?0:1;pass<2;++pass) {
    best=2147483647L;
    for(i=0;i<n;++i) {
      if(ink && (i==paper || i==background))continue;
      if(!pass && lum(page_palette[i])<LUM_DARK)continue;
      d=distance(page_palette[i],rgb);if(d<best){best=d;best_i=i;}
    }
    if(best!=2147483647L)break;
  }
  return best_i;
}


void screen_init(void)
{
  if(gfx_mode!=M_ZX){foreground=1;background=0;}
  hgfx_resume();
  screen_clear();
}

/* ---- display 4 (v9n): the PLATO page 1:1 in a window of 384 rows ----
 * gfx_woff = PLATO rows hidden above the window (0, 64, 128: top, middle, bottom); the window module
 * (ovl_hgw.asm) draws y' = 511 - woff - y and clips. Everything the host sends after a clear is
 * recorded in URAM 18-25 (ShowPLATO -> module), CS+3 / CS+4 (keyboard.c) move the window and
 * the module replays the page from the decoder state saved at the clear, without answers to the
 * host (rec_play, protocol.c). A new page starts at the top. Variables: gfxmain.asm. */
extern unsigned int gfx_woff;
extern unsigned char rec_play;
extern void win_ext(unsigned char f) __z88dk_fastcall;  /* module: 1 up, 2 down, 3 new page */
void hgfx_window(unsigned char down)
{
  if(gfx_mode==M_HGW)win_ext(down?2:1);
}

void screen_clear(void)
{
  if(con_tty && !logo_busy)con_tty_end();
  if(gfx_mode==M_HGW && !rec_play && !local_render)win_ext(3);
  hgfx_resume();
  if(gfx_mode!=M_ZX) {
    /* Clear first (index 0, synchronous incl. the FDMA), then install the new page
     * colours: the old picture never shows up in the new palette. */
    g_col=0;gfx_clear();
    palette_used=0;
    install_colour(0,background_rgb);background=0;paper=0;
    if(same_rgb(foreground_rgb,background_rgb))foreground=0;
    else {install_colour(1,foreground_rgb);foreground=1;}
    if(gfx_mode!=M_PAL)REG[14]=0;
    return;
  }
  /* Remember the page paper independently of later temporary colour changes. */
  paper=background;
  g_col=background;
  gfx_clear();
}

/* screen_beep: standard/snd.asm (v9u, any CPU speed) */
void screen_wait(void) { }

void hgfx_char_load(padWord charnum,charData data)
{
  if(charnum<0 || charnum>=128)return;
  hgfx_transpose((unsigned char *)data,user_font[charnum]);
  if(++g_cgen==0xff)g_cgen=0;
}

static unsigned char drawing_colour(void)
{
  /* Separate tests: some sccz80 versions miscompile an OR in a byte ternary. */
  if(CurMode==ModeErase)return background;
  if(CurMode==ModeInverse)return background;
  return foreground;
}

void screen_paint(padPt *p)
{
  if(CurMode==ModeInverse && gfx_mode!=M_ZX)return;   /* the ZX original paints in any mode */
  if(!fill_enabled || p->x<0 || p->x>511 || p->y<0 || p->y>511)return;
  hgfx_resume();
  if(gfx_mode==M_PAL){g_x1=p->x;g_y1=511-p->y;g_col=drawing_colour()&3;}
  else {
    g_x1=gfx_trx(p->x);g_y1=gfx_try(p->y);g_col=drawing_colour()&15;
    if(g_y1<0 || g_y1>=384)return;        /* display 4: seed outside the window */
  }
  gfx_fill();
}

static unsigned char *character(unsigned char c)
{
  if(CurMem==M0 || CurMem==M1) {
    c=(unsigned char)(c+((CurMem==M0)?-32:64));
    if(c>=160)return 0;
    return &hgfx_native_font[(unsigned int)c*16];
  }
  c=(unsigned char)(c+((CurMem==M2)?-32:32));
  if(c>=128)return 0;
  return user_font[c];
}

static void pixel(int x,int y,unsigned char colour)
{
  if(x<0 || x>511 || y<0 || y>511)return;
  hgfx_resume();
  g_x1=gfx_trx(x);g_y1=gfx_try(y);g_col=colour;
  gfx_pixel();
}

/* Slow path (Rotate or a cell crossing the canvas edge): PLATO semantics, bold = 2x2,
 * opaque modes paint the cell first. Pixels through the module. */
static void slow_chars(int ox,int oy,unsigned char *text,unsigned char count,
                       unsigned char ink,unsigned char cellpaper,unsigned char opaque)
{
  unsigned char *glyph;
  unsigned char row,col,bits,u,v,bold=ModeBold?2:1;
  int x,y,t;
  while(count--) {
    glyph=character(*text++);
    if(opaque) {
      int left=ox,right=ox+8*bold-1,bottom=oy,top=oy+16*bold-1;
      if(Rotate){left=ox-16*bold+1;right=ox;top=oy+8*bold-1;}
      if(left<0)left=0;
      if(right>511)right=511;
      if(bottom<0)bottom=0;
      if(top>511)top=511;
      if(left<=right && bottom<=top)
        rect(gfx_trx(left),gfx_try(top),gfx_trx(right),gfx_try(bottom),cellpaper);
    }
    if(glyph) {
      for(row=0;row<16;++row) {
        bits=glyph[row];
        for(col=0;col<8;++col) {
          if(!(bits&(0x80>>col)))continue;
          for(v=0;v<bold;++v)for(u=0;u<bold;++u) {
            x=col*bold+u;y=(15-row)*bold+v;
            if(Rotate){t=x;x=-y;y=t;}
            pixel(x+ox,y+oy,ink);
          }
        }
      }
    }
    if(Rotate)oy+=8*bold;else ox+=8*bold;
    if(!local_render)keyboard_main();
  }
}

/* Called by the module's screen_char_draw for Rotate and cells crossing the edge. */
void screen_char_draw_slow(padPt *coord,unsigned char *text,unsigned char count)
{
  unsigned char ink,cellpaper,opaque;
  ink=drawing_colour();cellpaper=background;opaque=0;
  if(CurMode==ModeRewrite){ink=foreground;cellpaper=background;opaque=1;}
  else if(CurMode==ModeInverse){ink=background;cellpaper=foreground;opaque=1;}
  slow_chars(coord->x,coord->y,text,count,ink,cellpaper,opaque);
}

/* ZX as the original: only the pure colours, otherwise white ink / black paper */
static unsigned char zx_rgb(const unsigned char *rgb,unsigned char other)
{
  unsigned char r=rgb[0],g=rgb[1],b=rgb[2];
  if((r!=0 && r!=255) || (g!=0 && g!=255) || (b!=0 && b!=255))return other;
  if(r && g && b)return other;
  return (unsigned char)((b?1:0)|(r?2:0)|(g?4:0));
}

void screen_foreground(padRGB *c)
{
  foreground_rgb[0]=c->red;foreground_rgb[1]=c->green;foreground_rgb[2]=c->blue;
  if(gfx_mode==M_ZX){foreground=zx_rgb(foreground_rgb,7);return;}
  foreground=page_colour(foreground_rgb,1);
}
void screen_background(padRGB *c)
{
  background_rgb[0]=c->red;background_rgb[1]=c->green;background_rgb[2]=c->blue;
  if(gfx_mode==M_ZX){background=zx_rgb(background_rgb,0);return;}
  background=page_colour(background_rgb,0);
}

static void help_text(int x,int y,const char *s)
{
  padPt p;
  p.x=x;p.y=y;
  screen_char_draw(&p,(unsigned char *)s,strlen(s));
}

/* HELP drawn in the graphics: the page is kept (URAM copy) and shown again afterwards. */
void hgfx_help_run(void)
{
  DispMode old_mode=CurMode;CharMem old_mem=CurMem;
  padBool old_bold=ModeBold,old_rotate=Rotate;
  unsigned char old_fg=foreground,old_bg=background,old_paper=paper,old_fill=fill_enabled;
  unsigned char key,i,pal=(gfx_mode==M_PAL);
  unsigned char old_page[16][3],old_fg_rgb[3],old_bg_rgb[3];
  unsigned int old_used=palette_used,old_woff=gfx_woff;
  memcpy(old_page,page_palette,sizeof(old_page));
  memcpy(old_fg_rgb,foreground_rgb,3);memcpy(old_bg_rgb,background_rgb,3);
  hgfx_resume();
  hgfx_help_restored=gfx_save();
  gfx_woff=0;
  foreground_rgb[0]=foreground_rgb[1]=foreground_rgb[2]=255;
  background_rgb[0]=background_rgb[1]=background_rgb[2]=0;
  fill_enabled=0;CurMode=ModeRewrite;CurMem=M0;
  ModeBold=padF;Rotate=padF;local_render=1;
  screen_clear();
  help_text(24,472,pal?"PLATOTERM PAL - PLATO KEY HELP":"PLATOTERM - PLATO KEY HELP");
  help_text(24,432,"ANS       EXTEND+A    BACK      EXTEND+B");
  help_text(24,408,"COPY      EXTEND+C    DATA      EXTEND+D");
  help_text(24,384,"EDIT      EXTEND+E    FONT      EXTEND+F");
  help_text(24,360,"DIVIDE    EXTEND+G    HELP      EXTEND+H");
  help_text(24,336,"LAB       EXTEND+L    MICRO     EXTEND+M");
  help_text(24,312,"SUPER     EXTEND+P    SQUARE    EXTEND+Q");
  help_text(24,288,"STOP      EXTEND+S    TERM      EXTEND+T");
  help_text(24,264,"SUB       EXTEND+Y    ASSIGN    EXTEND+1");
  help_text(24,240,"NEXT      ENTER       ERASE     DELETE");
  help_text(24,gfx_mode==M_HGW?200:64,"PRESS ENTER TO RETURN AND REDRAW SCREEN");
  if(!pal) {
    /* Cover the host's persistent bottom help/status row explicitly. */
    padPt a={0,0},b={511,31};
    CurMode=ModeErase;screen_block_draw(&a,&b);CurMode=ModeRewrite;
  }
  local_render=0;
  while(standard_raw_key());
  standard_input_reset();
  do{key=standard_getkey();}while(key!=13);
  keyboard_click();
  while(standard_raw_key());
  standard_input_reset();
  foreground=old_fg;background=old_bg;paper=old_paper;fill_enabled=old_fill;gfx_woff=old_woff;
  memcpy(foreground_rgb,old_fg_rgb,3);memcpy(background_rgb,old_bg_rgb,3);
  CurMode=old_mode;CurMem=old_mem;ModeBold=old_bold;Rotate=old_rotate;
  if(!pal){REG[14]=background;REG[15]=15;REG[13]=0;}
  if(hgfx_help_restored) {
    memcpy(page_palette,old_page,sizeof(old_page));palette_used=old_used;
    for(i=0;i<PAGE_N;++i)pal_palette_set(i,page_palette[i][0],page_palette[i][1],page_palette[i][2]);
    gfx_restore();
  }
}
