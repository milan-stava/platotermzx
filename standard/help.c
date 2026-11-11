#ifdef __SPECTRUM__
#include <spectrum.h>
#endif
#include <conio.h>
#include "help.h"
#include "keyboard-input.h"
#include "keyboard.h"
#include <ctype.h>
#include <string.h>
extern unsigned int standard_raw_key(void);

#define ENTRY_LENGTH 20

/** 
 * Clear help line
 */
void help_clear(void)
{
#ifdef __SPECTRUM__
  gotoxy(1,22);
  textcolor(WHITE);
  textbackground(BLACK);
  cprintf("                                                               ");
  gotoxy(1,22);
#endif
}

/**
 * Show the input prompt for help 
 */
void help_prompt_input(const char* text)
{
#ifdef __SPECTRUM__
  help_clear();
  textcolor(WHITE);
  textbackground(BLACK);
  cprintf(text);
#endif
}

/**
 * Show the resulting key gained from prompt input
 */
void help_prompt_key_display(const char* text, char extend)
{
#ifdef __SPECTRUM__
  gotoxy(32,22);
  cprintf(" - is: ");
  if (extend==1)
    cprintf("EXTEND ");
  cprintf(text);
#endif
}

/**
 * Show available PLATO keys to get help.
 */
void help_keys(void)
{
#ifdef __SPECTRUM__
  gotoxy(1,19);
  cprintf("                                                               ");
  cprintf("                                                               ");
  gotoxy(1,19);
  cprintf(" Available keys: ANS  BACK  COPY  DATA  EDIT  FONT  DIVIDE     ");
  cprintf(" HELP  LAB  MICRO  NEXT  SQUARE  STOP  SUB  SUPER  TERM  ASSIGN");
#endif  
}

/**
 * Return if entered string matches help entry
 */
int help_matches(char* entry, const char* match)
{
  unsigned char i=0;
  // Translate to uppercase.
  for (i=0;i<ENTRY_LENGTH;i++)
    {
      entry[i]=toupper(entry[i]);
    }
  return strcmp(entry,match);
}

/**
 * Help entrypoint 
 */
/* Direct matrix input, independent of ROM LAST_K. The caller is outside
 * all ESP transactions, so a user can spend any amount of time in help. */
static void help_release(void)
{
  while (standard_raw_key()) ;
  standard_input_reset();
}
static void help_read_entry(char *entry)
{
  unsigned char count=0, key;
  memset(entry,0,ENTRY_LENGTH);
  help_release();
  for (;;) {
    key=standard_getkey();
    if (key) keyboard_click();
    if (key==13) { putch('\n'); help_release(); return; }
    if (key==12 && count) {
      entry[--count]=0;
      putch(8); putch(' '); putch(8);
    } else if (key>=32 && key<127 && count<ENTRY_LENGTH-1) {
      entry[count++]=key;
      entry[count]=0;
      putch(key);
    }
  }
}

void help_run(void)
{
#ifdef __SPECTRUM__
  char help_done=1;
  char entry[ENTRY_LENGTH];
#ifdef __SPECTRUM__
  zx_border(YELLOW);
#endif
  help_keys();
    do {
      help_prompt_input(" Help for which key? ");
      help_read_entry(entry);
      if (help_matches(entry,"KEYS")==0)
	{
	  help_keys();
	  help_done=0; // Repeat help after keys.
	}
      else if (help_matches(entry,"ANS")==0)
	{
	  help_prompt_key_display("a",1);
	  help_done=1;
	}
      else if (help_matches(entry,"BACK")==0)
	{
	  help_prompt_key_display("b",1);
	  help_done=1;
	}
      else if (help_matches(entry,"COPY")==0)
	{
	  help_prompt_key_display("c",1);
	  help_done=1;
	} 
      else if (help_matches(entry,"DATA")==0)
	{
	  help_prompt_key_display("d",1);
	  help_done=1;
	}
      else if (help_matches(entry,"EDIT")==0)
	{
	  help_prompt_key_display("e",1);
	  help_done=1;
	}
      else if (help_matches(entry,"FONT")==0)
	{
	  help_prompt_key_display("f",1);
	  help_done=1;
	}
      else if (help_matches(entry,"DIVIDE")==0)
	{
	  help_prompt_key_display("g",1);
	  help_done=1;
	}
      else if (help_matches(entry,"HELP")==0)
	{
	  help_prompt_key_display("h",1);
	  help_done=1;
	}
      else if (help_matches(entry,"LAB")==0)
	{
	  help_prompt_key_display("l",1);
	  help_done=1;
	}
      else if (help_matches(entry,"MICRO")==0)
	{
	  help_prompt_key_display("m",1);
	  help_done=1;
	}
      else if (help_matches(entry,"SUPER")==0)
	{
	  help_prompt_key_display("p",1);
	  help_done=1;
	}
      else if (help_matches(entry,"NEXT")==0)
	{
	  help_prompt_key_display("ENTER",0);
	  help_done=1;
	}
      else if (help_matches(entry,"SQUARE")==0)
	{
	  help_prompt_key_display("q",1);
	  help_done=1;
	}
      else if (help_matches(entry,"ERASE")==0)
	{
	  help_prompt_key_display("DELETE",0);
	  help_done=1;
	}
      else if (help_matches(entry,"STOP")==0)
	{
	  help_prompt_key_display("s",1);
	  help_done=1;
	}
      else if (help_matches(entry,"TERM")==0)
	{
	  help_prompt_key_display("t",1);
	  help_done=1;
	}
      else if (help_matches(entry,"SUB")==0)
	{
	  help_prompt_key_display("y",1);
	  help_done=1;
	}
      else if (help_matches(entry,"ASSIGN")==0)
	{
	  help_prompt_key_display("1",1);
	  help_done=1;
	}
      else
	{
	  help_prompt_key_display(" not a PLATO key.",0);
	}
    }   while (help_done==0);
#ifdef __SPECTRUM__
    zx_border(BLACK);
#endif
#endif
}
