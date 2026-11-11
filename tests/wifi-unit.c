#include <stdlib.h>
#include <setjmp.h>
#define __MB_WIFI__
#define HGFX_IO_TEST
#ifdef STANDARD_WIFI_TEST
#include "../standard/wifi/io.c"
#else
#include "../src/spectrum/mb/io.c"
#endif

static const unsigned char *incoming;
static unsigned int incoming_size,incoming_pos,clock_tick,received_size,sent_size;
static unsigned char received[1024],sent[256];
static unsigned char expect_failure=0;
static unsigned char simulate_esp=0, configured_baud=7, esp_baud=7;
static unsigned char reported_baud=7, empty_commands=0, command_size=0;
static char probe_command[64];
static unsigned int baud_changes=0;
static unsigned char config_step=0, config_bad=0;
static const char *config_commands[]={"AT\r\n","ATE0\r\n","AT+CIPCLOSE=5\r\n",
  "AT+CIPCLOSE\r\n","AT+CWMODE_CUR=1\r\n","AT+CIPMUX=0\r\n",
  "AT+CIPMODE=0\r\n","AT+CIPDINFO=0\r\n","AT+CIPRECVMODE=1\r\n"};
static jmp_buf failure_target;
unsigned int in_Pause(unsigned int milliseconds) __z88dk_fastcall { return 0; }
unsigned int io_test_time(void) { return clock_tick++; }
void io_test_failure(void) {
  if (expect_failure) longjmp(failure_target,1);
  puts("FAIL: unexpected Wi-Fi error"); exit(1);
}
void mb_install(void) { }
int mb_current_baud(void) { return reported_baud; }
int mb_baud(unsigned char i) __z88dk_fastcall {
  configured_baud=i; ++baud_changes; command_size=0; return 0;
}
int mb_read(void) {
  if (incoming_pos==incoming_size) return -1;
  return incoming[incoming_pos++];
}
#ifdef STANDARD_WIFI_TEST
unsigned int mb_read_block(void *buffer,unsigned int count) {
  unsigned char *p=buffer;
  while(count) {
    int c=mb_read();
    if(c<0) return count;
    *p++=c; --count;
  }
  return 0;
}
void keyboard_main(void) { }
#endif
int mb_write(unsigned char c) __z88dk_fastcall {
  if (simulate_esp) {
    if (configured_baud!=esp_baud) return 0;
    if (command_size<sizeof(probe_command)-1) probe_command[command_size++]=c;
    if (c=='\n') {
      probe_command[command_size]=0;
      if (simulate_esp==2) {
        if (config_step>=9 || strcmp(probe_command,config_commands[config_step])) config_bad=1;
        ++config_step;
        if (!strncmp(probe_command,"AT+CIPCLOSE",11)) {
          incoming=(const unsigned char *)"\r\nERROR\r\n"; incoming_size=9;
        } else {
          incoming=(const unsigned char *)"\r\nOK\r\n"; incoming_size=6;
        }
      } else if (!strcmp(probe_command,"AT\r\n")) {
        incoming=(const unsigned char *)"\r\nOK\r\n";
        incoming_size=6;
      } else {
        ++empty_commands;
        incoming=(const unsigned char *)"\r\nERROR\r\n";
        incoming_size=9;
      }
      incoming_pos=command_size=0;
    }
    return 0;
  }
  if (sent_size>=sizeof(sent)) exit(1);
  sent[sent_size++]=c;
  return 0;
}
void ShowPLATO(padByte *p,unsigned short size) {
  if (size>sizeof(received)) exit(1);
#ifdef STANDARD_WIFI_TEST
  if (received_size+size>sizeof(received)) exit(1);
  memcpy(received+received_size,p,size); received_size+=size;
#else
  memcpy(received,p,size); received_size=size;
#endif
}
static void feed(const unsigned char *data,unsigned int size) {
  incoming=data; incoming_size=size; incoming_pos=0;
  received_size=sent_size=clock_tick=0;
  available_bytes=0;
#ifdef STANDARD_WIFI_TEST
  server_output_started=0;
  tx_count=0; tx_overflow=0;
#endif
  connected=1;
}
static void check(int condition,char *message) {
  if (!condition) { printf("FAIL: %s\n",message); exit(1); }
}
int main(void) {
  static const unsigned char binary[]={
    '+','C','I','P','R','E','C','V','L','E','N',':','9','\r','\n','O','K','\r','\n',
    '+','I','P','D',',','9','\r','\n',
    '+','C','I','P','R','E','C','V','D','A','T','A',',','9',':',
    0,255,'O','K','\r','\n','+',':',128,
    '\r','\n','O','K','\r','\n'
  };
  static const unsigned char alternate[]="+CIPRECVLEN:3\r\nOK\r\n+CIPRECVDATA:3,abc\r\nOK\r\n";
  static const unsigned char empty[]="+CIPRECVLEN:0\r\nOK\r\n";
#ifdef STANDARD_WIFI_TEST
  static const unsigned char too_big[]="+CIPRECVLEN:1025\r\nOK\r\n+CIPRECVDATA,1025:";
#else
  static const unsigned char too_big[]="+CIPRECVLEN:129\r\nOK\r\n+CIPRECVDATA,129:";
#endif
  static const unsigned char notice[]="+CIPRECVLEN:3\r\n+IPD,3\r\nOK\r\n+CIPRECVDATA,3:abc\r\nOK\r\n";
  static const unsigned char tx_answer[]="\r\n>\r\nSEND OK\r\n";
  char escaped[80];
#ifdef STANDARD_WIFI_TEST
  connected=0; io_initialized=0;
  mb_baud(7); /* mock returns HL=0 before connection activation */
  mark_connected();
  check(connected==1 && io_initialized==1,"both connection flags explicitly set");
  baud_changes=0;
#endif
  tcp_request(escaped,"IRATA.ONLINE",8005);
  check(!strcmp(escaped,"AT+CIPSTART=\"TCP\",\"IRATA.ONLINE\",8005\r\n"),
        "CIPSTART quotes and decimal port match exact wire format");
  /* Regression: alpha 2 sent an empty CR/LF command before AT. An ESP
   * rejecting the empty command makes result() return before the real OK. */
  feed((const unsigned char *)"\r\nERROR\r\n\r\nOK\r\n",15);
  check(!command("AT\r\n",50),"reproduce leading empty-command ERROR");
  feed((const unsigned char *)"",0);
  simulate_esp=1;
  check(probe_current_uart() && baud_changes==0 && !empty_commands,
        "probe current UART without changing baud or sending empty commands");
  feed((const unsigned char *)"",0);
  configured_baud=esp_baud=12; reported_baud=255; baud_changes=0;
  check(probe_current_uart() && configured_baud==12 && !baud_changes && !empty_commands,
        "preserve working UART even with unsupported baud readback");
  simulate_esp=2;
  feed((const unsigned char *)"",0);
  check(prepare_connection() && config_step==9 && !config_bad && !baud_changes,
        "working alpha6 initialization order without baud writes");
  simulate_esp=0;
#ifdef STANDARD_WIFI_TEST
  {
    static unsigned char bulk[1200];
    const char *prefix="+CIPRECVLEN:1027\r\nOK\r\n+CIPRECVDATA,1024:";
    const char *suffix="\r\nOK\r\n+CIPRECVDATA,3:xyz\r\nOK\r\n";
    unsigned int offset, j;
    strcpy(bulk,prefix); offset=strlen(prefix);
    for (j=0;j<1024;++j) bulk[offset+j]=(unsigned char)j;
    offset+=1024;
    strcpy(bulk+offset,suffix); offset+=strlen(suffix);
    feed(bulk,offset);
    io_main();
    check(received_size==1024 && available_bytes==3 && received[0]==0 &&
          received[255]==255 && received[1023]==255,"full 1 KB binary receive");
    sent_size=0;
    received_size=0;
    io_main();
    check(received_size==3 && !memcmp(received,"xyz",3) && !available_bytes,
          "drain cached backlog remainder");
    check(server_output_started,"first payload starts server display");
    check(sent_size==sizeof("AT+CIPRECVDATA=3\r\n")-1 &&
          !memcmp(sent,"AT+CIPRECVDATA=3\r\n",sizeof("AT+CIPRECVDATA=3\r\n")-1),
          "backlog avoids redundant pending length query");
  }
#endif
  quote_at(escaped,"name,with\"quote\\");
  check(!strcmp(escaped,"\"name\\,with\\\"quote\\\\\""),"escape AT quoted Wi-Fi fields");
  feed(binary,sizeof(binary)); io_main();
  check(received_size==9 && received[0]==0 && received[1]==255 &&
        received[6]=='+' && received[8]==128,"binary payload and IPD notices");
  check(incoming_pos==incoming_size,"consume final OK before rendering");
  feed(alternate,sizeof(alternate)-1); io_main();
  check(received_size==3 && !memcmp(received,"abc",3),"alternate ESP-AT framing");
  feed(notice,sizeof(notice)-1); io_main();
  check(received_size==3 && !memcmp(received,"abc",3),"IPD notice preserves pending length");
  feed(empty,sizeof(empty)-1); io_main();
  check(!received_size,"empty passive receive");
  check(sent_size==16 && !memcmp(sent,"AT+CIPRECVLEN?\r\n",16),
        "empty socket only queries length, never requests data");
  feed(tx_answer,sizeof(tx_answer)-1); io_send_byte(255);
#ifdef STANDARD_WIFI_TEST
  check(sent_size==0 && tx_count==1,"keyboard byte is queued without blocking");
  io_flush();
#endif
  check(sent_size==15 && sent[14]==255,"send binary key after prompt");
  check(!memcmp(sent,"AT+CIPSEND=1\r\n",14),"one byte CIPSEND command");
  feed((const unsigned char *)"CONNECT\r\nOK\r\n",13);
  check(command("AT+CIPSTART=\"TCP\",\"IRATA.ONLINE\",8005\r\n",1500),
        "accept CONNECT followed by final OK");
  feed((const unsigned char *)"CLOSED\r\nOK\r\n",12);
  check(command("AT+CIPCLOSE\r\n",100) && incoming_pos==incoming_size,
        "consume CLOSED and final OK before next command");
  feed((const unsigned char *)"DNS Fail\r\nERROR\r\n",17);
  check(!command("AT+CIPSTART\r\n",1500) &&
        !strcmp(error_reason,"ESP rejected command") && !strcmp(last_response,"DNS Fail"),
        "report rejected TCP connection");
  expect_failure=1;
  if (!setjmp(failure_target)) {
    feed(too_big,sizeof(too_big)-1); io_main();
    check(0,"reject oversized receive length");
  }
  puts("PASS: alpha6 initialization, preserved UART, passive length query, binary RX/TX");
  return 0;
}
