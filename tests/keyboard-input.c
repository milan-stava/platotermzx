#include <stdio.h>
#include <stdlib.h>
#define KEYBOARD_INPUT_TEST
#define NO_BIT
#include "../standard/keyboard-input.c"
#include "../standard/keyboard.c"
unsigned int keyboard_test_time;
static unsigned int raw;
static unsigned int calls;
static padWord key_sent;
padBool TTY=padF;
unsigned int standard_raw_key(void) { return raw; }
void Key(padWord key) { key_sent=key; ++calls; }
void io_send_byte(unsigned char key) { key_sent=key; ++calls; }
static unsigned int help_calls;
void help_run(void) { ++help_calls; }
static void check(int ok,char *message) {
  if(!ok) { printf("FAIL: %s\n",message); exit(1); }
}
int main(void) {
  standard_input_reset(); raw='g'; keyboard_main();
  check(calls==1 && key_sent==PKEY_g,"login letter g translation");
  keyboard_main(); check(calls==1,"held key does not flood");
  keyboard_test_time=24; keyboard_main(); check(calls==1,"repeat delay");
  keyboard_test_time=25; keyboard_main(); check(calls==2,"repeat starts");
  keyboard_test_time=29; keyboard_main(); check(calls==3,"repeat interval");
  raw=0; keyboard_main(); raw=10; keyboard_main();
  check(calls==4 && key_sent==PKEY_NEXT,"LF enter becomes PLATO NEXT");
  raw=0; keyboard_main(); raw=13; keyboard_main();
  check(calls==5 && key_sent==PKEY_NEXT,"CR enter becomes PLATO NEXT");
  raw=0; keyboard_main(); raw=8; keyboard_main();
  check(key_sent==PKEY_ERASE,"backspace becomes ERASE");
  raw=0; keyboard_main(); raw='G'; keyboard_main();
  check(key_sent==PKEY_G,"uppercase login letter");
  raw=0; keyboard_main(); TTY=padT; raw='a'; keyboard_main();
  check(key_sent=='a',"TTY sends raw letter");
  {
    unsigned int before=calls;
    raw=0; keyboard_main(); raw=14; keyboard_main();
    keyboard_test_time+=30; keyboard_main();
    check(is_extend==1 && calls==before,"held EXTEND stays local");
    raw=0; keyboard_main(); raw='0'; keyboard_main();
    check(!help_calls && calls==before,"help deferred outside UART waits, even in TTY");
    raw='x'; keyboard_main(); check(calls==before,"pending help absorbs local input");
    keyboard_service_local(); check(help_calls==1,"outer loop opens help");
    TTY=padF; raw=0; keyboard_main(); raw=14; keyboard_main();
    raw=0; keyboard_main(); raw='0'; keyboard_main();
    keyboard_service_local(); check(help_calls==2 && calls==before,"PLATO EXTEND 0 is local help");
  }
  puts("PASS: direct keyboard, login letters, NEXT, ERASE, repeat, TTY");
  return 0;
}
