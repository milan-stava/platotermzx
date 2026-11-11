/* Real PLATO decoder + real key translation + real ESP-AT driver.
 * Only hardware UART, key matrix and pixel output are simulated. No login
 * credentials are sent to a real service by this test. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __MB_WIFI__
#define HGFX_IO_TEST
#define KEYBOARD_INPUT_TEST
#define NO_BIT
#include "../standard/wifi/io.c"
#include "../standard/keyboard-input.c"
#include "../standard/keyboard.c"
#ifdef TEST_ORIGINAL_TERMINAL
#include "../standard/upstream/protocol.c"
#include "../standard/upstream/terminal.c"
#else
#include "../standard/protocol.c"
#include "../standard/terminal.c"
#endif

unsigned char already_started;
static unsigned int status_clears;
void screen_clear_status_row(void) { ++status_clears; }
unsigned char CharWide=8, CharHigh=16;
padPt TTYLoc;
unsigned char fontm23[768];
unsigned short fontptr[160];
unsigned int keyboard_test_time;
static unsigned int raw_key, ticks, rx_at, rx_size, server_at, server_size;
static unsigned int command_at, login_at, expect_data, login_stage;
static unsigned char uart[2048], server[2048], command_buf[100], login[64];
static char visible[2048];
static unsigned int visible_size, baud_writes, blocks;
static unsigned char capture_mode;
static unsigned char captured[512];
static unsigned int captured_size;
static unsigned char timed_keys;
static unsigned int timed_start, ack_at, clock_subticks;
static const char *setup_keys="y\r";
static void check(int ok,char *message) {
  if(!ok) { printf("FAIL: %s\n",message); exit(1); }
}
unsigned int standard_raw_key(void) {
  unsigned int t;
  if(!timed_keys) return raw_key;
  keyboard_test_time=ticks;
  t=ticks-timed_start;
  if(t<3) return 'a';
  if(t>=4 && t<7) return 'a';
  if(t>=8 && t<11) return '1';
  return 0;
}
int getch(void) {
  check(*setup_keys!=0,"setup must explicitly consume Y/N and host Enter");
  return *setup_keys++;
}
unsigned int in_Pause(unsigned int t) __z88dk_fastcall { return 0; }
unsigned int io_test_time(void) {
  if(timed_keys) {
    if(++clock_subticks==16) {clock_subticks=0;++ticks;}
    return ticks;
  }
  return ticks++;
}
void io_test_failure(void) {
  printf("FAIL: ESP step %s reason %s\n",stage,error_reason); exit(1);
}
void mb_install(void) { }
int mb_baud(unsigned char n) __z88dk_fastcall { ++baud_writes; return 0; }
int mb_current_baud(void) { return 7; }
int mb_read(void) {
  if(timed_keys && ticks<ack_at) return -1;
  return rx_at<rx_size?uart[rx_at++]:-1;
}
unsigned int mb_read_block(void *dest,unsigned int n) {
  unsigned char *p=dest;
  ++blocks;
  while(n) { int value=mb_read(); if(value<0) return n; *p++=value; --n; }
  return 0;
}
static void server_text(const char *p) {
  server_size=strlen(p); server_at=0; memcpy(server,p,server_size);
}
int mb_write(unsigned char byte) __z88dk_fastcall {
  unsigned int n;
  if(expect_data) {
    if(capture_mode) {
      check(captured_size<sizeof(captured),"captured keyboard bytes bound");
      captured[captured_size++]=byte;
      --expect_data;
      if(!expect_data) {
        strcpy(uart,"\r\nSEND OK\r\n"); rx_at=0; rx_size=strlen(uart);
        if(timed_keys) ack_at=ticks+8;
      }
      return 0;
    }
    check(login_at<sizeof(login),"login input bound");
    login[login_at++]=byte;
    --expect_data;
    if(!expect_data) {strcpy(uart,"\r\nSEND OK\r\n"); rx_at=0; rx_size=strlen(uart);}
    if(byte==13) {
      login[login_at-1]=0;
      if(login_stage<2)
        check(!strcmp(login,"guest"),"actual PLATO letters and NEXT sent as guest CR");
      else
        check(!strcmp(login,"aaz0199"),"synthetic password transmitted intact, no encryption");
      login_at=0; ++login_stage;
      if(login_stage==1) server_text("\x1b\x02group:");
      else if(login_stage==2) server_text("\x1b\x02password:");
      else server_text("\x1b\x02Welcome guest");
    }
    return 0;
  }
  check(command_at<sizeof(command_buf)-1,"AT command bound");
  command_buf[command_at++]=byte;
  if(byte!='\n') return 0;
  command_buf[command_at]=0;
  rx_at=rx_size=0;
  if(!strcmp(command_buf,"AT+CIPRECVLEN?\r\n")) {
    sprintf(uart,"+CIPRECVLEN:%u\r\nOK\r\n",server_size-server_at);
    rx_size=strlen(uart);
  } else if(!strncmp(command_buf,"AT+CIPRECVDATA=",15)) {
    n=atoi(command_buf+15);
    check(n>0 && n<=1024 && n<=server_size-server_at,"receive requested bound");
    sprintf(uart,"+CIPRECVDATA,%u:",n); rx_size=strlen(uart);
    memcpy(uart+rx_size,server+server_at,n); rx_size+=n; server_at+=n;
    strcpy(uart+rx_size,"\r\nOK\r\n"); rx_size+=6;
  } else if(!strncmp(command_buf,"AT+CIPSEND=",11)) {
    expect_data=atoi(command_buf+11);
    check(expect_data>0 && expect_data<=64,"key batch send length");
    strcpy(uart,"\r\n>"); rx_size=3;
  } else if(!strncmp(command_buf,"AT+CIPCLOSE",11)) {
    strcpy(uart,"ERROR\r\n"); rx_size=7;
  } else {
    strcpy(uart,"OK\r\n"); rx_size=4;
  }
  command_at=0;
  return 0;
}
static void append_text(unsigned char *p,unsigned int count) {
  check(visible_size+count<sizeof(visible),"visible text bound");
  memcpy(visible+visible_size,p,count); visible_size+=count; visible[visible_size]=0;
}
void screen_char_draw(padPt *p,unsigned char *text,unsigned char count) {
#ifdef REPLAY_RENDER
  unsigned char n;
  printf("TEXT mode=%u mem=%u xy=%d,%d: ",CurMode,CurMem,p->x,p->y);
  for(n=0;n<count;++n)printf("%02x ",text[n]);
  puts("");
#endif
  append_text(text,count);
}
void screen_tty_char(padByte byte) { if(byte>=32) append_text(&byte,1); }
void screen_init(void) { }
void screen_clear(void) { visible_size=0; visible[0]=0; }
void screen_wait(void) { }
void screen_beep(void) { }
void screen_dot_draw(padPt *p) { }
void screen_line_draw(padPt *a,padPt *b) { }
void screen_block_draw(padPt *a,padPt *b) {
#ifdef REPLAY_RENDER
  printf("BLOCK mode=%u xy=%d,%d to %d,%d\n",CurMode,a->x,a->y,b->x,b->y);
#endif
}
void screen_foreground(padRGB *c) { }
void screen_background(padRGB *c) { }
void screen_paint(padPt *p) { }
void touch_allow(padBool allow) { }
void help_run(void) { }
static void press(unsigned int key) {
  raw_key=key; keyboard_main();
  io_flush();
  raw_key=0; keyboard_main();
  keyboard_test_time+=2;
}
static void type_guest(void) {
  press('g'); press('u'); press('e'); press('s'); press('t'); press(10);
}
int main(void) {
  char selected_host[64];
#ifndef TEST_ORIGINAL_TERMINAL
  InitPAD();
  terminal_initial_position();
  ShowPLATO("\x1b\x82\x00\x1e" "A",5);
  check(status_clears==0,"probe A is not cleared before handshake");
  ShowPLATO("\x1bY\x50\x41\xc0",5);
  check(status_clears==1,"initial echo clears status row after probe");
  ShowPLATO("\x1bY\x50\x41\xc0",5);
  check(status_clears==1,"later echoes do not erase server content");
#endif
  setup_keys="\r"; select_server(selected_host);
  check(!strcmp(selected_host,"IRATA.ONLINE"),"default server is IRATA");
  setup_keys="2\r"; select_server(selected_host);
  check(!strcmp(selected_host,"cyberserv.org"),"second server selected");
  setup_keys=" \r"; select_server(selected_host);
  check(!strcmp(selected_host,"IRATA.ONLINE"),"SPACE does not change default selection");
  setup_keys="2 \r"; select_server(selected_host);
  check(!strcmp(selected_host,"cyberserv.org"),"SPACE does not change second selection");
  setup_keys="21\r"; select_server(selected_host);
  check(!strcmp(selected_host,"IRATA.ONLINE"),"explicit return to IRATA");
  setup_keys="y\r";
  InitPAD(); standard_input_reset();
  io_init();
  check(standard_enable_fill && !*setup_keys,"Y fill selection and host prompt consumed");
  check(!baud_writes,"UART speed preserved");
  check(connected==1 && io_initialized==1,"full io_init activates connection");
  server_text("\x1b\x02login:");
  io_main(); check(strstr(visible,"login:")!=0 && !TTY,"real decoder shows login in PLATO mode");
  type_guest(); check(login_stage==1,"first login submitted");
  io_main(); check(strstr(visible,"group:")!=0,"group prompt decoded");
  type_guest(); check(login_stage==2,"group submitted");
  io_main(); check(strstr(visible,"password:")!=0,"password prompt decoded");
  {
    unsigned int before=visible_size;
    press('a');press('a');press('z');press('0');press('1');press('9');press('9');
    check(visible_size==before && !strstr(visible,"aaz0199"),"typed password has no local screen echo");
  }
  press(10); check(login_stage==3,"password submitted with one NEXT");
  io_main(); check(strstr(visible,"Welcome guest")!=0,"welcome response decoded");
  check(blocks==4,"four responses use block RX");
  {
    unsigned int k, mode;
    capture_mode=1;
    for(mode=0;mode<2;++mode) {
      FlowControl=mode;
      for(k=32;k<127;++k) {
        if(!((k>='a' && k<='z') || (k>='A' && k<='Z') || (k>='0' && k<='9'))) continue;
        captured_size=0; press(k);
        check(captured_size==1 && captured[0]==k,"alphanumeric byte identity in both PLATO mappings");
      }
    }
    FlowControl=padF;
    puts("PASS: a-z, A-Z, 0-9 transmitted unchanged in both PLATO mappings");
    captured_size=0;
    standard_input_reset();
    timed_keys=1; timed_start=ticks; ack_at=0;
    while((unsigned int)(ticks-timed_start)<30 || tx_count) {
      keyboard_main(); io_flush(); ++ticks;
    }
    timed_keys=0;
    printf("Timed keys captured (%u):",captured_size);
    {unsigned int j;for(j=0;j<captured_size;++j)printf(" %02X",captured[j]);}puts("");
    check(captured_size==3 && !memcmp(captured,"aa1",3),
          "short release and repeated keys survive delayed ESP send ACK");
    capture_mode=0;
  }
  setup_keys="n\r";
  io_init();
  check(!standard_enable_fill && !*setup_keys,"N disables fill, dialog is not skipped");
  setup_keys="\r\r"; io_init();
  check(standard_enable_fill && !*setup_keys,"visible fill prompt defaults to Y on Enter");
  puts("PASS: login -> group -> hidden password aaz0199 -> NEXT -> welcome, actual protocol and TX");
  return 0;
}
