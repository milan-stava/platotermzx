/* Exercise the real application startup and keyboard loop as Z80 code. */
#include <stdio.h>
#include <stdlib.h>
#include "../standard/upstream/io.h"
#include "../standard/upstream/keyboard.h"
static unsigned char dummy_splash[1];
#define splash dummy_splash
#define main application_main
#include "../standard/main.c"
#undef main
#undef splash
static unsigned int keys;
static unsigned char ready_tone;
void screen_init(void) { }
void terminal_init(void) { }
void terminal_ready(void) { }
void terminal_initial_position(void) { }
void ShowPLATO(padByte *p,unsigned short n) { }
void io_init(void) { }
void standard_input_reset(void) { }
void keyboard_service_local(void) { }
void bit_play(char *melody) {
  if (melody[0]!='A' || melody[1]!='2') exit(1);
  ready_tone=1;
}
void keyboard_main(void) {
  if (++keys>100) { puts("FAIL: keyboard loop never reaches Wi-Fi"); exit(1); }
}
void io_main(void) {
  if (!ready_tone || !keys) exit(1);
  printf("PASS: application reaches Wi-Fi after %u keyboard calls and ready tone\n",keys);
  exit(0);
}
int main(void) { application_main(); return 1; }
