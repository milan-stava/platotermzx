#define main integration_main
#include "login-integration.c"
#undef main
#include "font-reference.h"
int main(void)
{
  charData bitmap;
  unsigned char expected[6];
  unsigned int pattern,column,seed=17,glyph;
  for(glyph=0;glyph<128;++glyph)fontptr[glyph]=glyph*6;
  for(pattern=0;pattern<384;++pattern) {
    glyph=pattern & 127;
    for(column=0;column<8;++column) {
      seed=seed*25173+13849;
      if(pattern<129) {
        unsigned int bit; bitmap[column]=0;
        for(bit=0;bit<16;++bit)
          if(column*16+bit<pattern)bitmap[column]|=1U<<bit;
      } else bitmap[column]=seed;
    }
    reference_char_load(glyph,bitmap);
    memcpy(expected,fontm23+glyph*6,6);
    terminal_char_load(glyph,bitmap);
    if(memcmp(expected,fontm23+glyph*6,6)) {
      printf("Pattern %u reference density %u\n",pattern,(unsigned int)pix_cnt);
      for(column=0;column<6;++column)printf("%02x/%02x ",expected[column],fontm23[glyph*6+column]);
      puts("");
      check(0,"optimized glyph matches per-glyph reference raster");
    }
  }
  puts("PASS: font raster equivalence, all 0..128 pixel densities, randomized glyphs, all 128 slots");
  return 0;
}
