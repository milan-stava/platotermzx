#include <input.h>
#include "keyboard-input.h"
static unsigned char previous;
static unsigned int pressed_at;
static unsigned char repeat_delay;
extern unsigned int standard_raw_key(void);
static unsigned int input_time(void)
{
#ifdef KEYBOARD_INPUT_TEST
  extern unsigned int keyboard_test_time;
  return keyboard_test_time;
#else
  return *(volatile unsigned int *)23672;
#endif
}
void standard_input_reset(void)
{
  previous=0;
  repeat_delay=25;
}
unsigned char standard_getkey(void)
{
  unsigned int raw=standard_raw_key();
  unsigned int tick=input_time();
  unsigned char key;
  if (!raw || raw>255) { previous=0; return 0; }
  key=raw;
  /* The original PLATO translation table expects CR and ERASE as 13/12. */
  if (key==10) key=13;
  if (key==8 || key==127) key=12;
  if (key!=previous) {
    previous=key;
    pressed_at=tick;
    repeat_delay=25;
    return key;
  }
  if ((unsigned int)(tick-pressed_at)<repeat_delay) return 0;
  pressed_at=tick;
  repeat_delay=4;
  return key;
}
