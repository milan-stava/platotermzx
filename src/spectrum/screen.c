#ifdef __SPECTRUM__

#include <graphics.h>
#include <sound.h>
#include <stdlib.h>
#include <spectrum.h>
#include "../include/screen.h"
#include "../include/protocol.h"
#include "../include/keyboard.h"

extern unsigned char CharWide;
extern unsigned char CharHigh;
extern padPt TTYLoc;
extern padBool FastText; /* protocol.c */
extern unsigned short scalex[];
extern unsigned short scaley[];
extern unsigned char font[];
extern unsigned char fontm23[];
extern unsigned short fontptr[];
extern unsigned char FONT_SIZE_X;
extern unsigned char FONT_SIZE_Y;
extern char enable_fill;

long foregroundColor=INK_WHITE;
long backgroundColor=PAPER_BLACK;

#if defined(__MBEL__) && !defined(MBEL_SLOW_TEXT)
/* FastText only changes set glyph pixels. Merge a whole glyph row instead
 * of calling the generic graphics and attribute routines for each pixel. */
static void text_row(unsigned int x, unsigned int y, unsigned char bits,
                     unsigned char ink) __naked
{
#asm
  EXTERN __zx_console_attr
  push ix
  ld ix,0
  add ix,sp
  ld h,(ix+6)
  ld l,0
  ld a,(ix+10)
  and 7
  jr z,mbel_row_shifted
  ld b,a
mbel_row_shift:
  srl h
  rr l
  djnz mbel_row_shift
mbel_row_shifted:
  ld c,h
  ld b,l
  ld a,(ix+8)
  and 7
  or 64
  ld h,a
  ld a,(ix+8)
  and 192
  rrca
  rrca
  rrca
  or h
  ld h,a
  ld a,(ix+8)
  and 56
  rlca
  rlca
  ld l,a
  ld a,(ix+10)
  rrca
  rrca
  rrca
  and 31
  or l
  ld l,a
  ld a,(ix+4)
  or a
  jr z,mbel_row_erase
  ld a,c
  or (hl)
  ld (hl),a
  inc hl
  ld a,b
  or (hl)
  ld (hl),a
  jr mbel_row_attrs
mbel_row_erase:
  ld a,c
  cpl
  and (hl)
  ld (hl),a
  inc hl
  ld a,b
  cpl
  and (hl)
  ld (hl),a
mbel_row_attrs:
  ld a,(ix+8)
  inc a
  call mbel_row_attraddr
  ld a,c
  and 254
  jr z,mbel_row_attrnext
  ld a,(_foregroundColor)
  ld (hl),a
mbel_row_attrnext:
  inc hl
  ld a,c
  and 1
  or b
  jr z,mbel_row_current
  ld a,(_foregroundColor)
  ld (hl),a
mbel_row_current:
  ld a,(ix+8)
  call mbel_row_attraddr
  ld a,c
  or a
  jr z,mbel_row_second
  ld a,(__zx_console_attr)
  ld (hl),a
mbel_row_second:
  inc hl
  ld a,b
  or a
  jr z,mbel_row_done
  ld a,(__zx_console_attr)
  ld (hl),a
mbel_row_done:
  pop ix
  ret
mbel_row_attraddr:
  ld e,a
  and 192
  rlca
  rlca
  add a,88
  ld h,a
  ld a,e
  and 56
  rlca
  rlca
  ld l,a
  ld a,(ix+10)
  rrca
  rrca
  rrca
  and 31
  or l
  ld l,a
  ret
#endasm
}
#endif

/**
 * screen_init() - Set up the screen
 */
void screen_init(void)
{
  clg();
}

/**
 * screen_beep(void) - Beep the terminal
 */
void screen_beep(void)
{
#ifdef __MBEL__
  /* Bounded integer parameters: avoid the floating-point frequency wrapper. */
  bit_beep(30,500);
#else
  bit_frequency(.2,440);
#endif
}

/**
 * screen_clear - Clear the screen
 */
void screen_clear(void)
{
  clg();
  zx_colour(PAPER_BLACK|INK_WHITE);
}

/**
 * screen_char_draw(Coord, ch, count) - Output buffer from ch* of length count as PLATO characters
 */
void screen_char_draw(padPt* Coord, unsigned char* ch, unsigned char count)
{
  short offset; /* due to negative offsets */
  unsigned short x;      /* Current X and Y coordinates */
  unsigned short y;
  unsigned short* px;   /* Pointers to X and Y coordinates used for actual plotting */
  unsigned short* py;
  unsigned char i; /* current character counter */
  unsigned char a; /* current character byte */
  unsigned char j,k; /* loop counters */
  char b; /* current character row bit signed */
  unsigned char width=FONT_SIZE_X;
  unsigned char height=FONT_SIZE_Y;
  unsigned short deltaX=1;
  unsigned short deltaY=1;
  unsigned char mainColor=1;
  unsigned char altColor=0;
  unsigned char *p;
  unsigned char* curfont;
  unsigned char* aaddr;
  
  switch(CurMem)
    {
    case M0:
      curfont=font;
      offset=-32;
      break;
    case M1:
      curfont=font;
      offset=64;
      break;
    case M2:
      curfont=fontm23;
      offset=-32;
      break;
    case M3:
      curfont=fontm23;
      offset=32;      
      break;
    }

  if (CurMode==ModeRewrite)
    {
      altColor=0;
    }
  else if (CurMode==ModeInverse)
    {
      altColor=1;
    }
  
  if (CurMode==ModeErase || CurMode==ModeInverse)
    mainColor=0;
  else
    mainColor=1;

  x=scalex[(Coord->x)];
  y=scaley[(Coord->y)+15];
  
  if (FastText==padF)
    {
      goto chardraw_with_fries;
    }

  /* the diet chardraw routine - fast text output. */
  
  for (i=0;i<count;++i)
    {
      #ifdef __SPECTRUM__
      keyboard_main();  // Spectrum additional keyboard checks when processing rxbuffer
      #endif
      
      a=*ch;
      ++ch;
      a+=offset;
      p=&curfont[fontptr[a]];
      
      for (j=0;j<FONT_SIZE_Y;++j)
  	{
  	  b=*p;

#if defined(__MBEL__) && !defined(MBEL_SLOW_TEXT)
          if (FONT_SIZE_X==4 && x+FONT_SIZE_X<256 && y<191) {
            text_row(x,y,(unsigned char)b & 240,mainColor);
            ++y;
            ++p;
            continue;
          }
#endif
	  
  	  for (k=0;k<FONT_SIZE_X;++k)
  	    {
  	      if (b<0) /* check sign bit. */
		{
		  *zx_pxy2aaddr(x+1,y+1)=foregroundColor;
		  if (mainColor==0)
		    unplot(x,y);
		  else
		    plot(x,y);
		}

	      ++x;
  	      b<<=1;
  	    }

	  ++y;
	  x-=width;
	  ++p;
  	}

      x+=width;
      y-=height;
    }

  return;

 chardraw_with_fries:
  if (Rotate)
    {
      deltaX=-abs(deltaX);
      width=-abs(width);
      px=&y;
      py=&x;
    }
    else
    {
      px=&x;
      py=&y;
    }
  
  if (ModeBold)
    {
      deltaX = deltaY = 2;
      width<<=1;
      height<<=1;
    }
  
  for (i=0;i<count;++i)
    {
      #ifdef __SPECTRUM__
      keyboard_main();  // Spectrum additional keyboard checks when processing rxbuffer
      #endif

      a=*ch;
      ++ch;
      a+=offset;
      p=&curfont[fontptr[a]];
      for (j=0;j<FONT_SIZE_Y;++j)
  	{
  	  b=*p;

	  if (Rotate)
	    {
	      px=&y;
	      py=&x;
	    }
	  else
	    {
	      px=&x;
	      py=&y;
	    }

  	  for (k=0;k<FONT_SIZE_X;++k)
  	    {
  	      if (b<0) /* check sign bit. */
		{
		  if (ModeBold)
		    {
		      if (mainColor==0)
			{
			  *zx_pxy2aaddr(*px+1,*py)=foregroundColor;
			  *zx_pxy2aaddr(*px,*py+1)=foregroundColor;
			  *zx_pxy2aaddr(*px+1,*py+1)=foregroundColor;
			  unplot(*px+1,*py);
			  unplot(*px,*py+1);
			  unplot(*px+1,*py+1);
			}
		      else
			{
			  *zx_pxy2aaddr(*px+1,*py)=foregroundColor;
			  *zx_pxy2aaddr(*px,*py+1)=foregroundColor;
			  *zx_pxy2aaddr(*px+1,*py+1)=foregroundColor;
			  plot(*px+1,*py);
			  plot(*px,*py+1);
			  plot(*px+1,*py+1);
			}
		    }
		  *zx_pxy2aaddr(*px,*py)=foregroundColor;
		  if (mainColor==0)
		    unplot(*px,*py);
		  else
		    plot(*px,*py);
		}
	      else
		{
		  if (CurMode==ModeInverse || CurMode==ModeRewrite)
		    {
		      if (ModeBold)
			{
			  if (altColor==0)
			    {
			      *zx_pxy2aaddr(*px+1,*py)=foregroundColor;
			      *zx_pxy2aaddr(*px,*py+1)=foregroundColor;
			      *zx_pxy2aaddr(*px+1,*py+1)=foregroundColor;
			      unplot(*px+1,*py);
			      unplot(*px,*py+1);
			      unplot(*px+1,*py+1);
			    }
			  else
			    {
			      *zx_pxy2aaddr(*px+1,*py)=foregroundColor;
			      *zx_pxy2aaddr(*px,*py+1)=foregroundColor;
			      *zx_pxy2aaddr(*px+1,*py+1)=foregroundColor;
			      plot(*px+1,*py);
			      plot(*px,*py+1);
			      plot(*px+1,*py+1);
			    }
			}
		      *zx_pxy2aaddr(*px,*py);
		      if (altColor==0)
			unplot(*px,*py);
		      else
			plot(*px,*py);      
		    }
		}

	      x += deltaX;
  	      b<<=1;
  	    }

	  y+=deltaY;
	  x-=width;
	  ++p;
  	}

      Coord->x+=width;
      x+=width;
      y-=height;
    }

  return;
}

/**
 * screen_foreground - Set foreground
 */
void screen_foreground(padRGB* theColor)
{
  unsigned char red=theColor->red;
  unsigned char green=theColor->green;
  unsigned char blue=theColor->blue;
  
  if (red==0 && green==0 && blue==0)
    {
      foregroundColor=INK_BLACK;
    }
  else if (red==0 && green==0 && blue==255)
    {
      foregroundColor=INK_BLUE;
    }
  else if (red==0 && green==255 && blue==0)
    {
      foregroundColor=INK_GREEN;
    }
  else if (red==255 && green==0 && blue==0)
    {
      foregroundColor=INK_RED;
    }
  else if (red==0 && green==255 && blue==255)
    {
      foregroundColor=INK_CYAN;
    }
  else if (red==255 && green==0 && blue==255)
    {
      foregroundColor=INK_MAGENTA;
    }
  else if (red==255 && green==255 && blue==0)
    {
      foregroundColor=INK_YELLOW;
    }
  else
    {
      foregroundColor=INK_WHITE;
    }
}

/**
 * screen_background - Set Background
 */
void screen_background(padRGB* theColor)
{
  unsigned char red=theColor->red;
  unsigned char green=theColor->green;
  unsigned char blue=theColor->blue;
  
  if (red==0 && green==0 && blue==0)
    {
      backgroundColor=PAPER_BLACK;
    }
  else if (red==0 && green==0 && blue==255)
    {
      backgroundColor=PAPER_BLUE;
    }
  else if (red==0 && green==255 && blue==0)
    {
      backgroundColor=PAPER_GREEN;
    }
  else if (red==255 && green==0 && blue==0)
    {
      backgroundColor=PAPER_RED;
    }
  else if (red==0 && green==255 && blue==255)
    {
      backgroundColor=PAPER_CYAN;
    }
  else if (red==255 && green==0 && blue==255)
    {
      backgroundColor=PAPER_MAGENTA;
    }
  else if (red==255 && green==255 && blue==0)
    {
      backgroundColor=PAPER_YELLOW;
    }
  else
    {
      backgroundColor=PAPER_BLACK;
    }
}

#endif /* __SPECTRUM__ */
