/* Offline cost probe only; never linked into the TAP. Reuse the integration
 * harness for the actual upstream decoder/terminal and hardware stubs. */
#define main login_integration_main
#include "login-integration.c"
#undef main
#ifndef STARTUP_GLYPHS
#define STARTUP_GLYPHS 128
#endif
int main(void)
{
  unsigned int glyph, column;
  charData bitmap;
  for(glyph=0;glyph<128;glyph++) fontptr[glyph]=glyph*6;
  for(column=0;column<8;column++) bitmap[column]=0x33cc;
  for(glyph=0;glyph<STARTUP_GLYPHS;glyph++)
    terminal_char_load(glyph & 127,bitmap);
  printf("Offline font conversion: %u glyphs\n",(unsigned int)STARTUP_GLYPHS);
  return 0;
}
