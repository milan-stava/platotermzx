#include <stdbool.h>
#include <input.h>
#include <stdint.h>
#include "protocol.h"
#include "screen.h"
#include "terminal.h"
#include "connect.h"
#include "splash.h"
#include "io.h"
#include "keyboard.h"
#include "keyboard-input.h"

#include "sound.h"	//Added sound for Keyboard and ready beep

unsigned char already_started=0;

void main(void)
{
#ifdef __SPECTRUM__
  zx_border(INK_BLACK);  //Tidy up the borders on start up
#endif
  standard_input_reset();
#ifndef __TESTTARGET__
  __asm
    ei
  __endasm;
#endif
  screen_init();
  terminal_init();
  ShowPLATO(splash,sizeof(splash));
  terminal_ready();
  terminal_initial_position();
#ifdef __SPECTRANET__
  connect();
#endif
  io_init();

#ifndef NO_BIT
  bit_play("A2");  //Ready beep
#endif
  
  for (;;)
    {
      keyboard_main();

      io_main();
      keyboard_service_local();
    }
}
