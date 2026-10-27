#ifdef __MBEL__
#include <conio.h>
#include <input.h>
#include <string.h>
#include <spectrum.h>
#include "../../include/io.h"
#include "../../include/protocol.h"
#include "../../include/keyboard.h"
#include "../../include/help.h"
#include "esp.h"
#include "uart.h"
#ifdef __HGFX__
#include "hgfx.h"
#endif

static unsigned char rxdata[ESP_CHUNK];
static unsigned char txdata[256];
static unsigned int txcount;
static char host[64];
static char ssid[33];
static char password[64];
char io_initialized;
extern unsigned char mbel_saved_flags;

void io_done(void)
{
    if (io_initialized) esp_close();
    io_initialized = 0;
    txcount = 0;
#ifdef __HGFX__
    hgfx_suspend();
#endif
    textcolor(BLACK);
    textbackground(WHITE);
    clrscr();
    zx_border(7);
    /* Restore BASIC keyboard flags and normal permanent attributes. */
    bpoke(0x5c3b, mbel_saved_flags);
    bpoke(0x5c48, 56);
    bpoke(0x5c8d, 56);
    bpoke(0x5c8e, 0);
    bpoke(0x5c8f, 56);
    bpoke(0x5c90, 0);
}

/* Bounded input; never write credentials or host names to a disk. */
static void input(char *s, unsigned char capacity, unsigned char secret)
{
    unsigned char used = 0;
    int c;
    for (;;) {
        c = getch();
        if (c == 10 || c == 13) break;
        if ((c == 8 || c == 12) && used) {
            --used;
            cprintf("\b \b");
        } else if (c >= 32 && c <= 126 && used + 1 < capacity) {
            s[used++] = c;
            putch(secret ? '*' : c);
        }
    }
    s[used] = 0;
}

static void join_wifi(void)
{
    help_prompt_input("Wi-Fi SSID: ");
    input(ssid, sizeof(ssid), 0);
    help_prompt_input("Wi-Fi password: ");
    input(password, sizeof(password), 1);
    help_prompt_input("Connecting to Wi-Fi...");
    esp_join(ssid, password);
    memset(password, 0, sizeof(password));
}

void io_init(void)
{
    int c;
#ifdef __HGFX__
    hgfx_suspend();
#endif
    if (!host[0]) {
        wifi_uart_init();
        /* MB03+ may have just powered its ESP on. Interrupts stay enabled. */
        in_Pause(1500);
        help_prompt_input("Host (ENTER = IRATA.ONLINE): ");
        input(host, sizeof(host), 0);
        if (!host[0]) strcpy(host, "IRATA.ONLINE");
    }
    io_initialized = 0;
    txcount = 0;
    for (;;) {
        help_prompt_input("Connecting...");
        if (esp_open(host)) break;
        help_clear();
        cprintf("ESP error %u. R=retry W=Wi-Fi", esp_error);
        do { c = getch(); } while (c != 'r' && c != 'R' && c != 'w' && c != 'W');
        if (c == 'w' || c == 'W') join_wifi();
    }
    help_clear();
    io_initialized = 1;
#ifdef __HGFX__
    hgfx_resume();
#endif
}

static int flush(void)
{
    if (!txcount) return 1;
    if (!esp_send(txdata, txcount)) return 0;
    txcount = 0;
    return 1;
}

void io_send_byte(unsigned char b)
{
    if (!io_initialized || esp_error || esp_closed) return;
    /* ShowPLATO/keyboard can emit several bytes for a single PLATO key.
     * Batch them into one CIPSEND; flush a full queue without dropping bytes.
     */
    if (txcount == sizeof(txdata) && !flush()) return;
    txdata[txcount++] = b;
}

void io_main(void)
{
    int n;
    keyboard_main();
    if (esp_error || !flush()) goto disconnected;
    n = esp_recv(rxdata);
    if (n < 0) goto disconnected;
    if (n) {
        ShowPLATO(rxdata, n);
        if (esp_error || !flush()) goto disconnected;
    } else if (esp_closed) {
        goto disconnected;
    } else {
        in_Pause(1);
    }
    return;
disconnected:
    io_initialized = 0;
#ifdef __HGFX__
    hgfx_suspend();
#endif
    help_clear();
    cprintf("Connection ended (%u). R=reconnect", esp_error);
    while ((n = getch()) != 'r' && n != 'R');
    io_init();
    InitTTY();
}
#endif
