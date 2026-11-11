#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../standard/screen-clear.c"
static unsigned char memory[6914];
int main(void) {
  unsigned int x,y,offset;
  memset(memory,0xa5,sizeof(memory));
  standard_clear_lower_screen(memory+1);
  if(memory[0]!=0xa5 || memory[6913]!=0xa5)exit(1);
  for(y=0;y<192;++y)for(x=0;x<32;++x) {
    offset=((y&192)<<5)|((y&7)<<8)|((y&56)<<2)|x;
    if(memory[1+offset]!=(y<96?0xa5:0))exit(1);
  }
  for(y=0;y<24;++y)for(x=0;x<32;++x)
    if(memory[1+6144+y*32+x]!=(y<12?0xa5:7))exit(1);
  puts("PASS: lower-screen clear, every pixel/attribute byte, upper title and guards preserved");
  memset(memory,0xa5,sizeof(memory));
  standard_clear_last_row(memory+1);
  if(memory[0]!=0xa5 || memory[6913]!=0xa5)exit(1);
  for(y=0;y<192;++y)for(x=0;x<32;++x) {
    offset=((y&192)<<5)|((y&7)<<8)|((y&56)<<2)|x;
    if(memory[1+offset]!=(y<184?0xa5:0))exit(1);
  }
  for(y=0;y<24;++y)for(x=0;x<32;++x)
    if(memory[1+6144+y*32+x]!=(y<23?0xa5:7))exit(1);
  puts("PASS: handover clears only the last row, all other bytes preserved");
  return 0;
}
