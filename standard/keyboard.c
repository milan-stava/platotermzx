/**
 * PLATOTerm64 - A PLATO Terminal for the Commodore 64
 * Based on Steve Peltz's PAD
 * 
 * Author: Thomas Cherryhomes <thom.cherryhomes at gmail dot com>
 *
 * keyboard_base.c - Keyboard functions (base)
 */

#ifdef __SPECTRUM__
#include <conio.h>
#include <spectrum.h>
#endif
#include "key.h"
#include "keyboard.h"
#include "keyboard-input.h"
#include "screen.h"
#include "protocol.h"
#include "io.h"
#include "plato_key.h"
#include "help.h"

#include <stdlib.h>

extern padBool TTY;
static unsigned char ch;
extern unsigned int standard_raw_key(void);
unsigned char is_extend=0;  //deleted static is used in IO.C now for Rasta bars
/* One event per idle scan burst. Without this guard a held Mahjong control
 * can repeat several times before the server has redrawn the first move. */
unsigned char keyboard_scan_event=0;
static unsigned char help_pending;
#ifdef __HGFX__
extern void hgfx_help_run(void);
extern unsigned char hgfx_help_restored;
extern void hgfx_shutdown(void);
#endif
#ifdef __PAL__
extern void hgfx_extend_indicator(unsigned char active);
#define PAL_EXTEND_INDICATOR(active) hgfx_extend_indicator(active)
#else
#define PAL_EXTEND_INDICATOR(active) ((void)0)
#endif

/* Called by the outer loop, never from an ESP response wait. */
#ifdef __HGFX__
/* v9n display 4: CS+3 (code 128) / CS+4 (129) move the window; run here (outer loop) as HELP */
static unsigned char window_pending;
extern void hgfx_window(unsigned char down);
#endif
void keyboard_service_local(void)
{
#ifdef __HGFX__
  if (window_pending) {
    unsigned char k=window_pending;
    window_pending=0;
    hgfx_window(k==129);
    standard_input_reset();
  }
#endif
  if (!help_pending) return;
  help_pending=0;
#ifdef __HGFX__
  hgfx_help_run();
  /* The help screen restores the page from its URAM copy. Only when there
   * was no free URAM ask the host to redraw it (DATA), as before. */
  if(!hgfx_help_restored) keyboard_out(PKEY_DATA);
#else
  help_run();
#endif
  standard_input_reset();
}

/**
 * A simple key press feedback.
 */
/* keyboard_click: standard/snd.asm (v9u, for any CPU speed) */

/**
 * keyboard_out - If platoKey < 0x7f, pass off to protocol
 * directly. Otherwise, platoKey is an access key, and the
 * ACCESS key must be sent, followed by the particular
 * access key from PTAT_ACCESS.
 */
void keyboard_out(unsigned char platoKey)
{
  if (platoKey==0xff)
    return;
  
  if (platoKey>0x7F)
    {
      Key(ACCESS);
      Key(ACCESS_KEYS[platoKey-0x80]);
      return;
    }
  Key(platoKey);
  return;
}

/**
 * keyboard_out_tty - keyboard output to serial I/O in TTY mode
 */
void keyboard_out_tty(char ch)
{
  io_send_byte(ch);  	// *IRQ-OFF  (SENDING DATA)
}

/**
 * keyboard_main - Handle the keyboard presses
 */
void keyboard_main(void)
{
  if (help_pending) return;
  /* Original author's C keyboard path.  This gave the shortest first-key
   * response on real PLATOTerm/server use.  Autorepeat is intentionally not
   * synthesized here; the server cannot consume repeated keys as quickly as
   * the local matrix scanner can generate them. */
  ch=getk();
  if (ch!=0x00)
    {
      keyboard_scan_event=1;
#ifndef NO_BIT
      keyboard_click(); // maybe something better? ;) -thom
#endif
      if (ch==0x0e) // EXTEND pressed; repeats must not become remote keys.
		{
          if (is_extend)
            {
              is_extend=0;
#ifdef __SPECTRUM__
              zx_border(INK_BLACK);
#endif
              PAL_EXTEND_INDICATOR(0);
              return;
            }
#ifdef __SPECTRUM__
			zx_border(INK_GREEN);
#endif
			PAL_EXTEND_INDICATOR(1);
			is_extend=1;
		}
      else if (is_extend==1 && ch==0x30)
		{
		  is_extend=0;
			PAL_EXTEND_INDICATOR(0);
			help_pending=1;
		}
#ifdef __HGFX__
      else if (is_extend==1 && (ch=='x' || ch=='X'))
		{
		  hgfx_shutdown();
		  exit(0);
		}
#endif
#ifdef __HGFX__
      else if (ch==128 || ch==129)
		{
		  is_extend=0;
		  PAL_EXTEND_INDICATOR(0);
		  window_pending=ch;
		}
#endif
      else if (TTY)
        {
          is_extend=0;
          PAL_EXTEND_INDICATOR(0);
          keyboard_out_tty(ch);
        }
      else if (is_extend==1 && ch==0x39)
	{
#ifdef __SPECTRUM__
	  zx_border(INK_MAGENTA);
	  zx_hardcopy();
	  zx_border(INK_BLACK);
	  is_extend=0;
	  PAL_EXTEND_INDICATOR(0);
#endif
	}
#ifdef LAT_TEST
      else if (is_extend==1 && (ch=='l' || ch=='L'))
		{
		  extern void lat_show(void);
		  is_extend=0;
		  PAL_EXTEND_INDICATOR(0);
		  lat_show();
		}
#endif
      else if (is_extend==1)
		{
#ifdef __SPECTRUM__
			zx_border(INK_GREEN);
#endif
#ifdef LAT_TEST
			{ extern void lat_key(void); lat_key(); }
#endif
			keyboard_out(extend_key_to_pkey[ch]);
			is_extend=0;
			PAL_EXTEND_INDICATOR(0);
#ifdef __SPECTRUM__
			zx_border(INK_BLACK);
#endif
		}
      else
		{
#ifdef __SPECTRUM__
			zx_border(INK_BLACK);
#endif
#ifdef LAT_TEST
			{ extern void lat_key(void); lat_key(); }
#endif
			keyboard_out(key_to_pkey[ch]);
		}
    }
 //  ELSE here for No Keypress events
}

/**
 * keyboard_clear() - Clear the keyboard buffer
 */
void keyboard_clear(void)
{
}
