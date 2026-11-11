#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define KEYBOARD_INPUT_TEST
#include "../standard/keyboard-input.c"
#include "../standard/help.c"
unsigned int keyboard_test_time;
static const unsigned char *sequence;
static unsigned int remaining;
static unsigned int clicks;
void keyboard_click(void) { ++clicks; }
unsigned int standard_raw_key(void) {
  ++keyboard_test_time;
  if(!remaining){puts("FAIL: help input stalled");exit(1);}
  --remaining;
  return *sequence++;
}
static void check(int ok,char *message) {
  if(!ok){printf("FAIL: %s\n",message);exit(1);}
}
int main(void) {
  static const unsigned char keys[]={0,'n',0,'e',0,'x',0,'x',0,8,0,'t',0,13,0};
  unsigned char longkeys[65];
  char entry[ENTRY_LENGTH+1];
  unsigned int i;
  sequence=keys;remaining=sizeof(keys);
  help_read_entry(entry);
  check(!strcmp(entry,"next") && !remaining,"direct help input and ERASE, no ROM LAST_K");
  check(clicks==7,"one click for each help key including ERASE and Enter");
  check(help_matches(entry,"NEXT")==0,"case-insensitive help lookup returns int");
  longkeys[0]=0;
  for(i=1;i<61;i++)longkeys[i]=(i&1)?'a':0;
  longkeys[61]=13;longkeys[62]=0;
  sequence=longkeys;remaining=63;entry[ENTRY_LENGTH]=0x55;
  help_read_entry(entry);
  check(strlen(entry)==ENTRY_LENGTH-1 && entry[ENTRY_LENGTH]==0x55 && !remaining,"help input bound and Enter release");
  puts("PASS: help direct matrix input, deletion, lookup, bounds, release");
  return 0;
}
