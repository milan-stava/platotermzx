#ifdef __SPECTRUM__

#include <stdbool.h>
#include <input.h>
#include <stdint.h>
#include <stdio.h>
#include "../include/protocol.h"
#include "../include/screen.h"
#include "../include/terminal.h"
#include "../include/connect.h"
#include "splash.h"
#include "../include/help.h"
#include <conio.h>

#include "../include/io.h"
#include "../include/keyboard.h"
#ifdef __HGFX__
#include "mbel/hgfx.h"
#endif

#include "sound.h"	//Added sound for Keyboard and ready beep

unsigned char already_started=0;

void main(void)
{
  bpoke(0x5C3B, bpeek(0x5C3B) | 0x8); /*Alistair's workaround to autoboot key mode issue */
  char c; 
  zx_border(INK_BLACK);  //Tidy up the borders on start up
  screen_init();
#ifndef __HGFX__
  terminal_init();
  ShowPLATO(splash,sizeof(splash));
  terminal_ready();
  terminal_initial_position();
#else
#ifdef __HGFX3__
  cprintf("PLATOTerm - HGFX 3 / 512x384\r\n");
#else
  cprintf("PLATOTerm - HGFX 5 / 720x576\r\n");
#endif
  cprintf("T = offline graphics test, ENTER = connect\r\n");
  c = getch();
  if (c == 't' || c == 'T') {
    hgfx_demo();
    in_WaitForNoKey();
    getch();
    hgfx_suspend();
    screen_init();
  }
#endif
  
  help_clear();
  cprintf("Enable graphics fill?(y/n)");
  while(1)
  {
    in_Pause(1); // Slow this loop
    c = getch();
    if (c == 'y') {
      enable_fill = 1;
      break;
    }
    if (c == 'n')  {
      enable_fill = 0;
      break;
    }
  }

#ifdef __SPECTRANET__
  connect();
#endif
#ifdef __UNO__
  connect();
#endif
  io_init();

#ifdef __HGFX__
  hgfx_start();
  terminal_init();
  ShowPLATO(splash,sizeof(splash));
  terminal_ready();
  terminal_initial_position();
#endif

#ifndef NO_BIT
#ifdef __HGFX__
  screen_beep();
#else
  bit_play("2A--");  //Ready beep
#endif
#endif

  for (;;)
    {  
      /* CHANGES TO THE MAIN LOOP
          We now only check the keyboard on NO RX data 
          or while drawing on the screen, 
          except during flood fills.
      */
      

      #ifdef __UNO__  //Not tuned the UNO sorry...
      for(int Kscan=0;Kscan<20;Kscan++)  //Keyboard scanning loop		
      {
	      keyboard_main();
      }
      #endif
      
      io_main();
    }
}

#endif /* __SPECTRUM__ */
