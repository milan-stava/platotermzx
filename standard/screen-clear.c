#include <string.h>
#include "screen-clear.h"
/* Character row 23: pixels 184..191, attributes 736..767. */
void standard_clear_last_row(unsigned char *screen)
{
  unsigned char band;
  for(band=0;band<8;++band) memset(screen+0x10e0+((unsigned int)band<<8),0,32);
  memset(screen+0x1ae0,7,32);
}
/* Spectrum bitmap and attributes at screen[0..6911]. Clear pixels y=96..191
 * and attribute rows 12..23, preserving the upper title exactly. */
void standard_clear_lower_screen(unsigned char *screen)
{
  unsigned char band;
  for(band=0;band<8;++band) memset(screen+0x880+((unsigned int)band<<8),0,128);
  memset(screen+0x1000,0,2048);
  memset(screen+0x1980,7,384);
}
