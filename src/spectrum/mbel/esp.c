/* ESP-AT single TCP connection, passive receive, length-framed binary data.
 * There are no calls into the renderer from this file. Each UART transaction
 * consumes its complete response before returning, so keyboard/protocol output
 * cannot re-enter a half-read AT response while ShowPLATO is rendering.
 */
#if defined(__MBEL__) || defined(MBEL_HOST_TEST)
#include <string.h>
#include <stdint.h>
#include "uart.h"
#include "esp.h"

unsigned char esp_error;
unsigned char esp_closed;
static uint16_t started;
static unsigned int timeout_frames = 500;
static unsigned int available;
static char line[96];
static char command[224];

static void begin(void)
{
    started = wifi_ticks();
}

static int timed_out(void)
{
    if ((uint16_t)(wifi_ticks() - started) >= timeout_frames) {
        esp_error = ESP_TIMEOUT;
        return 1;
    }
    return 0;
}

static int read_byte(void)
{
    int c;
    do {
        if (timed_out()) return -1;
        c = wifi_uart_read();
        if (c == -2) {
            esp_error = ESP_UART_ERROR;
            return -1;
        }
        if (c >= 0) return c;
    } while (1);
}

static int write_byte(unsigned char c)
{
    do {
        if (timed_out()) return 0;
        if (wifi_uart_write(c)) return 1;
    } while (1);
}

static int write_text(const char *s)
{
    while (*s) if (!write_byte(*s++)) return 0;
    return 1;
}

/* Decimal parsing is deliberately independent of atoi's signed 16-bit range. */
static int number(const char *s, unsigned int *out)
{
    unsigned int n = 0;
    unsigned char digit;
    if (*s < '0' || *s > '9') return 0;
    do {
        digit = *s++ - '0';
        if (n > 6553 || (n == 6553 && digit > 5)) return 0;
        n = n * 10 + digit;
    } while (*s >= '0' && *s <= '9');
    if (*s && *s != ',') return 0;
    *out = n;
    return 1;
}

static char *decimal(char *p, unsigned int n)
{
    char digits[5];
    unsigned char i = 0;
    do {
        digits[i++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (i) *p++ = digits[--i];
    *p = 0;
    return p;
}

/* Handle both +CIPRECVDATA,<len>: (older ESP-AT) and
 * +CIPRECVDATA:<len>, (newer ESP-AT). Never scan binary payload for tokens.
 */
static int payload(unsigned char delimiter, unsigned char *data,
                   unsigned int capacity, unsigned int *actual)
{
    unsigned int n, i;
    unsigned char used = 0;
    int c;
    char digits[6];
    for (;;) {
        c = read_byte();
        if (c < 0) return 0;
        if (c == delimiter) break;
        if (c < '0' || c > '9' || used == 5) goto bad;
        digits[used++] = c;
    }
    digits[used] = 0;
    if (!number(digits, &n) || !data || n > capacity) goto bad;
    for (i = 0; i < n; ++i) {
        c = read_byte();
        if (c < 0) return 0;
        data[i] = c;
    }
    *actual = n;
    return 1;
bad:
    esp_error = ESP_FRAMING;
    return 0;
}

static int reply(unsigned char *data, unsigned int capacity,
                 unsigned int *actual, unsigned char prompt)
{
    unsigned char used = 0, truncated = 0, got_data = 0;
    int c;
    for (;;) {
        c = read_byte();
        if (c < 0) return 0;
        if (prompt && c == '>' && used == 0) return 1;
        if (c == '\r') continue;
        if (c == '\n') {
            line[used] = 0;
            if (!strcmp(line, "OK")) {
                if (prompt) { used = 0; continue; }
                if (data && !got_data) goto bad_frame;
                return 1;
            }
            if (!strcmp(line, "ERROR") || !strcmp(line, "FAIL") ||
                !strcmp(line, "SEND FAIL") || !strncmp(line, "busy", 4) ||
                !strcmp(line, "WIFI DISCONNECT") || !strcmp(line, "ready")) {
                esp_error = ESP_AT_ERROR;
                return 0;
            }
            if (!strcmp(line, "CLOSED")) esp_closed = 1;
            if (!strncmp(line, "+CIPRECVLEN:", 12)) {
                if (truncated || !number(line + 12, &available)) goto bad_frame;
            }
            if (!strcmp(line, "SEND OK")) return 1;
            used = truncated = 0;
            continue;
        }
        if (used < sizeof(line) - 1) line[used++] = c;
        else truncated = 1;
        if (used == 13 && !memcmp(line, "+CIPRECVDATA", 12)) {
            if (got_data || (c != ',' && c != ':')) goto bad_frame;
            if (!payload(c == ',' ? ':' : ',', data, capacity, actual)) return 0;
            used = 0;
            got_data = 1;
        }
    }
bad_frame:
    esp_error = ESP_FRAMING;
    return 0;
}

static int issue(const char *s)
{
    begin();
    return write_text(s) && reply(0, 0, 0, 0);
}

/* Recover a transparent session left by another application. Guard intervals
 * use ROM frames rather than CPU-speed-dependent delay loops. Only called
 * before opening our socket; no active-session FIFO clears are performed.
 */
static int guard(void)
{
    uint16_t start = wifi_ticks();
    while ((uint16_t)(wifi_ticks() - start) < 60) {
        if (wifi_uart_read() == -2) { esp_error = ESP_UART_ERROR; return 0; }
    }
    return 1;
}

static int probe(void)
{
    if (issue("AT\r\n")) return 1;
    if (esp_error == ESP_UART_ERROR) return 0;
    esp_error = 0;
    if (!guard()) return 0;
    begin();
    if (!write_text("+++")) return 0;
    if (!guard()) return 0;
    return issue("AT\r\n");
}

/* Quote ESP AT arguments, including literal comma, quotation mark and slash. */
static char *quoted(char *p, const char *s, unsigned int limit)
{
    unsigned int used = 0;
    *p++ = '"';
    while (*s) {
        if (++used > limit || (unsigned char)*s < 32 || (unsigned char)*s > 126)
            return 0;
        if (*s == ',' || *s == '"' || *s == '\\') *p++ = '\\';
        *p++ = *s++;
    }
    *p++ = '"';
    *p = 0;
    return p;
}

int esp_join(const char *ssid, const char *password)
{
    char *p;
    int ok;
    esp_error = 0;
    strcpy(command, "AT+CWJAP_CUR=");
    p = quoted(command + strlen(command), ssid, 32);
    if (!p) goto invalid;
    *p++ = ',';
    p = quoted(p, password, 63);
    if (!p) goto invalid;
    strcpy(p, "\r\n");
    timeout_frames = 1500;
    ok = issue(command);
    timeout_frames = 500;
    return ok;
invalid:
    esp_error = ESP_FRAMING;
    return 0;
}

int esp_open(const char *host)
{
    char *p;
    esp_error = esp_closed = 0;
    if (!probe() || !issue("ATE0\r\n")) return 0;
    /* Closing an absent socket returns ERROR. Both commands are intentional:
     * an earlier application may have left either single or multi mode set.
     */
    issue("AT+CIPCLOSE=5\r\n");
    if (esp_error == ESP_UART_ERROR || esp_error == ESP_TIMEOUT) return 0;
    esp_error = 0;
    issue("AT+CIPCLOSE\r\n");
    if (esp_error == ESP_UART_ERROR || esp_error == ESP_TIMEOUT) return 0;
    esp_error = esp_closed = 0;
    if (!issue("AT+CWMODE_CUR=1\r\n") || !issue("AT+CIPMUX=0\r\n") ||
        !issue("AT+CIPMODE=0\r\n") || !issue("AT+CIPDINFO=0\r\n") ||
        !issue("AT+CIPRECVMODE=1\r\n")) return 0;
    strcpy(command, "AT+CIPSTART=\"TCP\",");
    p = quoted(command + strlen(command), host, 63);
    if (!p) { esp_error = ESP_FRAMING; return 0; }
    strcpy(p, ",8005\r\n");
    return issue(command);
}

int esp_send(const unsigned char *data, unsigned int count)
{
    char *p;
    unsigned int i;
    if (esp_error || esp_closed) return 0;
    strcpy(command, "AT+CIPSEND=");
    p = decimal(command + strlen(command), count);
    strcpy(p, "\r\n");
    begin();
    if (!write_text(command) || !reply(0, 0, 0, 1)) return 0;
    for (i = 0; i < count; ++i) if (!write_byte(data[i])) return 0;
    return reply(0, 0, 0, 0);
}

int esp_recv(unsigned char *data)
{
    unsigned int actual = 0, count;
    char *p;
    if (esp_error) return -1;
    available = 65535;
    if (!issue("AT+CIPRECVLEN?\r\n")) return -1;
    if (available == 65535) { esp_error = ESP_FRAMING; return -1; }
    if (!available) return 0;
    count = available > ESP_CHUNK ? ESP_CHUNK : available;
    strcpy(command, "AT+CIPRECVDATA=");
    p = decimal(command + strlen(command), count);
    strcpy(p, "\r\n");
    begin();
    if (!write_text(command) || !reply(data, count, &actual, 0)) return -1;
    return actual;
}
#endif
