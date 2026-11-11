/* ESP-AT passive TCP reception. Requires an AP saved by Wi-Fi Setup.
 * Passive mode keeps binary PLATO data out of AT command responses and lets
 * the ESP buffer network traffic while the Z80 renders HGFX.
 */
#ifdef __MB_WIFI__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <input.h>
#ifndef HGFX_IO_TEST
#include <spectrum.h>
#endif
#include "../../include/io.h"
#include "../../include/protocol.h"
#include "../../include/screen.h"

extern void mb_install(void);
extern int mb_baud(unsigned char index) __z88dk_fastcall;
extern int mb_current_baud(void);
extern int mb_read(void);
extern int mb_write(unsigned char byte) __z88dk_fastcall;
char io_initialized=0;
static unsigned char payload[128];
static char line[100];
static unsigned char connected=0;
static const char *stage="startup";
static const char *error_reason="timeout";
static char last_response[100];
static unsigned int available_bytes;
static unsigned char length_ready;

static int parse_available(const char *p)
{
  unsigned int value=0;
  unsigned char digits=0;
  while (*p>='0' && *p<='9') {
    if (++digits>5 || value>6553 || (value==6553 && *p>'5')) return 0;
    value=value*10+*p++-'0';
  }
  if (!digits || (*p && *p!=',')) return 0;
  available_bytes=value;
  length_ready=1;
  return 1;
}

static unsigned int now(void)
{
#ifdef HGFX_IO_TEST
  extern unsigned int io_test_time(void);
  return io_test_time();
#else
  /* FRAMES is updated by the Spectrum IM1 interrupt at 50 Hz. */
  return *(volatile unsigned int *)23672;
#endif
}

static int receive(unsigned int frames)
{
  unsigned int start=now();
  int c;
  do {
    c=mb_read();
    if (c!=-1) {
      if (c<0) error_reason="UART FIFO overflow";
      return c;
    }
  } while ((unsigned int)(now()-start)<frames);
  return -1;
}

static int send_text(const char *s)
{
  while (*s) if (mb_write(*s++)) {
    error_reason="UART transmit timeout";
    return 0;
  }
  return 1;
}

static int result(unsigned int frames)
{
  unsigned int start=now();
  unsigned char n=0, truncated=0;
  int c;
  while ((unsigned int)(now()-start)<frames) {
    c=mb_read();
    if (c==-1) continue;
    if (c<0) { error_reason="UART FIFO overflow"; return 0; }
    if (c=='\r') continue;
    if (c=='\n') {
      line[n]=0;
      if (truncated) { n=truncated=0; continue; }
      if (!strncmp(line,"+CIPRECVLEN:",12) && !parse_available(line+12)) {
        error_reason="invalid pending TCP length";
        return 0;
      }
      /* CIPCLOSE may emit CLOSED before its final OK. Consume both before
       * issuing the next command, otherwise the old OK can acknowledge it. */
      if (!strcmp(line,"CLOSED") && !strncmp(stage,"AT+CIPCLOSE",11)) {
        n=0;
        continue;
      }
      if (!strcmp(line,"OK") || !strcmp(line,"SEND OK")) return 1;
      if (!strcmp(line,"ERROR") || !strcmp(line,"FAIL") ||
          !strcmp(line,"SEND FAIL") || !strcmp(line,"CLOSED") ||
          !strncmp(line,"busy ",5)) {
        error_reason="ESP rejected command";
        if (!last_response[0]) strcpy(last_response,line);
        return 0;
      }
      if (n) strcpy(last_response,line);
      n=0;
    } else if (n<sizeof(line)-1) line[n++]=c;
    else truncated=1;
  }
  return 0;
}

static int command(const char *s, unsigned int frames)
{
  stage=s;
  error_reason="ESP response timeout";
  last_response[0]=0;
  return send_text(s) && result(frames);
}

static void show_error(void)
{
  connected=0;
#ifdef __HGFX__
  hgfx_console();
#endif
  printf("\nPLATOTerm HGFX alpha 4\nWi-Fi error: %s\nStep: %s\n",error_reason,stage);
  if (last_response[0]) printf("ESP: %s\n",last_response);
}

static void failure(void)
{
#ifdef HGFX_IO_TEST
  extern void io_test_failure(void);
  io_test_failure();
#else
  show_error();
  printf("Restart PLATOTerm to reconnect.\n");
  for (;;) ;
#endif
}

static int probe_current_uart(void)
{
  unsigned int drain;
  unsigned char attempt;
  /* Drain by reading, without the core-specific FIFO-reset port. An empty
   * CR/LF command can return ERROR ahead of AT's OK: send only AT here. */
  for (attempt=0; attempt<2; ++attempt) {
    for (drain=0; drain<4096; ++drain)
      if (mb_read()==-1) break;
    if (drain==4096) {
      error_reason="UART receive drain limit";
      return 0;
    }
    if (command("AT\r\n",50)) return 1;
  }
  return 0;
}

static int prepare_connection(void)
{
  if (!probe_current_uart() || !command("ATE0\r\n",100)) return 0;
  /* Same command order as the supplied working MB/EL alpha6 TAP.
   * Closing an absent connection may return ERROR; consume and ignore it. */
  command("AT+CIPCLOSE=5\r\n",100);
  command("AT+CIPCLOSE\r\n",100);
  if (!command("AT+CWMODE_CUR=1\r\n",100)) return 0;
  if (!command("AT+CIPMUX=0\r\n",100)) return 0;
  if (!command("AT+CIPMODE=0\r\n",100)) return 0;
  if (!command("AT+CIPDINFO=0\r\n",100)) return 0;
  return command("AT+CIPRECVMODE=1\r\n",100);
}

static void input_text(char *text, unsigned char capacity, unsigned char secret)
{
  unsigned char n=0;
  int c;
  while ((c=getch())!=13) {
    if ((c==8 || c==12 || c==127) && n) {
      --n; putch(8); putch(' '); putch(8);
    } else if (c>=32 && c<=126 && n<capacity-1) {
      text[n++]=c;
      putch(secret?'*':c);
    }
  }
  text[n]=0;
  putch('\n');
}

static char *quote_at(char *dest, const char *value)
{
  *dest++='"';
  while (*value) {
    if (*value=='"' || *value=='\\' || *value==',') *dest++='\\';
    *dest++=*value++;
  }
  *dest++='"';
  *dest=0;
  return dest;
}

static int join_wifi(void)
{
  char ssid[33], password[64], request[224], *p;
  int ok;
  printf("Wi-Fi SSID: "); input_text(ssid,sizeof(ssid),0);
  printf("Wi-Fi password: "); input_text(password,sizeof(password),1);
  strcpy(request,"AT+CWJAP_CUR=");
  p=quote_at(request+strlen(request),ssid);
  *p++=',';
  p=quote_at(p,password);
  strcpy(p,"\r\n");
  printf("Connecting to Wi-Fi...\n");
  if (!command("AT+CWMODE_CUR=1\r\n",100)) ok=0;
  else ok=command(request,1500);
  /* Never print the AT command carrying credentials in error diagnostics. */
  stage="join Wi-Fi access point";
  last_response[0]=0;
  memset(password,0,sizeof(password));
  memset(request,0,sizeof(request));
  return ok;
}

static void tcp_request(char *dest, const char *host, unsigned int port_number)
{
  sprintf(dest,"AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",host,port_number);
}

void io_init(void)
{
  char host[64], request[100], *port;
  unsigned int port_number;
  int c;
  mb_install();
  printf("PLATOTerm HGFX alpha 4 - 512x384 / 16\nWi-Fi BIOS 2.1 - Element / MB03+\n");
  printf("UART speed unchanged (as reference alpha6).\n");
  in_Pause(1500);
  printf("Host[:port] [IRATA.ONLINE:8005]: ");
  input_text(host,sizeof(host),0);
  if (!host[0]) strcpy(host,"IRATA.ONLINE");
  /* Restrict host syntax before embedding it in an AT command. */
  for (port=host; *port; ++port)
    if (!( (*port>='a' && *port<='z') || (*port>='A' && *port<='Z') ||
           (*port>='0' && *port<='9') || *port=='.' || *port=='-' || *port==':')) {
      stage="host input"; error_reason="invalid host name"; failure();
    }
  port=strchr(host,':');
  port_number=8005;
  if (port) {
    *port++=0;
    port_number=0;
    while (*port>='0' && *port<='9') {
      if (port_number>6553 || (port_number==6553 && *port>'5')) break;
      port_number=port_number*10+*port++-'0';
    }
    if (*port || !port_number || !host[0]) {
      stage="host:port input"; error_reason="invalid TCP port"; failure();
    }
  }
  tcp_request(request,host,port_number);
  for (;;) {
    printf("\nConnecting to %s:%u...\n",host,port_number);
    if (prepare_connection() && command(request,1500)) break;
    show_error();
    printf("R=retry  W=enter Wi-Fi SSID/password\n");
    do { c=getch(); } while (c!='r' && c!='R' && c!='w' && c!='W');
    if (c=='w' || c=='W') {
      if (!join_wifi()) show_error();
    }
  }
  connected=io_initialized=1;
}

void io_send_byte(unsigned char b)
{
  unsigned int start;
  int c;
  if (!connected) return;
  stage="TCP send / CIPSEND prompt";
  error_reason="ESP response timeout";
  last_response[0]=0;
  if (!send_text("AT+CIPSEND=1\r\n")) failure();
  start=now();
  do {
    c=mb_read();
    if (c==-2) failure();
    if (c=='>') break;
  } while ((unsigned int)(now()-start)<100);
  if (c!='>' || mb_write(b) || !result(250)) failure();
}

void io_main(void)
{
  const char *header="+CIPRECVDATA";
  unsigned int start, length=0, i, requested;
  unsigned char matched=0, digits=0;
  char request[28];
  int c;
  if (!connected) return;
  length_ready=0;
  /* alpha6 first queries the pending length. Empty sockets are normal and
   * must not issue CIPRECVDATA blindly: some firmware returns ERROR then. */
  if (!command("AT+CIPRECVLEN?\r\n",100)) failure();
  if (!length_ready) {
    error_reason="missing pending TCP length";
    failure();
  }
  if (!available_bytes) { in_Pause(10); return; }
  requested=available_bytes;
  if (requested>sizeof(payload)) requested=sizeof(payload);
  sprintf(request,"AT+CIPRECVDATA=%u\r\n",requested);
  stage="TCP receive / CIPRECVDATA";
  error_reason="ESP receive framing or timeout";
  last_response[0]=0;
  if (!send_text(request)) failure();
  start=now();
  while (matched<12) {
    c=receive(100);
    if (c<0 || (unsigned int)(now()-start)>250) failure();
    if (c==header[matched]) ++matched;
    else matched=(c=='+')?1:0;
  }
  /* ESP-AT variants use either ,<length>: or :<length>,. */
  c=receive(100);
  if (c!=',' && c!=':') failure();
  for (;;) {
    c=receive(100);
    if (c>='0' && c<='9') {
      if (++digits>3) failure();
      length=length*10+c-'0';
      if (length>requested) failure();
    } else break;
  }
  if (!digits || (c!=':' && c!=',')) failure();
  for (i=0; i<length; ++i) {
    c=receive(100);
    if (c<0) failure();
    payload[i]=c;
  }
  if (!result(100)) failure();
  if (length) ShowPLATO(payload,length);
  else in_Pause(10);
}
#endif
