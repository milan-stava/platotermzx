/* Real ESP parser/command code, simulated UART only. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/spectrum/mbel/esp.h"

static unsigned char queue[8192], fixture[ESP_CHUNK];
static unsigned int head, tail, ticks, calls, wire_length, sent, queries, reads;
static unsigned int device_available;
static int mode, overflow, stall, reading, command_len;
static char command[512];

static void append(const void *data, unsigned int n)
{
    assert(tail + n < sizeof(queue));
    memcpy(queue + tail, data, n);
    tail += n;
}
static void text(const char *s) { append(s, strlen(s)); }

unsigned int wifi_ticks(void) { return (ticks++ / 16) & 65535; }
void wifi_uart_init(void) {}
int wifi_uart_read(void)
{
    if (overflow) return -2;
    /* Fragment headers and data at many different byte boundaries. */
    if (stall || (++calls % 8) == 0 || head == tail) return -1;
    return queue[head++];
}
int wifi_uart_write(unsigned char c)
{
    unsigned int n;
    char header[80];
    if (reading) {
        ++sent;
        if (--reading == 0) text("\r\nSEND OK\r\n");
        return 1;
    }
    assert(command_len + 1 < (int)sizeof(command));
    command[command_len++] = c;
    command[command_len] = 0;
    if (c != '\n') return 1;
    if (!strcmp(command, "AT+CIPRECVLEN?\r\n")) {
        ++queries;
        if (mode == 5) text("\r\nOK\r\n"); /* Missing length. */
        else if (mode == 6) text("\r\n+CIPRECVLEN:999999\r\nOK\r\n");
        else if (mode == 7) text("\r\nERROR\r\n");
        else {
            sprintf(header, "\r\n+CIPRECVLEN:%u,0,0,0,0\r\nOK\r\n", device_available);
            text(header);
        }
    } else if (sscanf(command, "AT+CIPRECVDATA=%u", &n) == 1) {
        ++reads;
        assert(n <= ESP_CHUNK && n <= device_available);
        wire_length = mode == 3 ? n + 1 : n;
        if (mode == 1)
            sprintf(header, "\r\n+CIPRECVDATA:%u,", wire_length);
        else
            sprintf(header, "\r\n+CIPRECVDATA,%u:", wire_length);
        text(header);
        append(fixture, n);
        device_available -= n;
        if (mode == 2) text("\r\nCLOSED\r\n");
        text("\r\nOK\r\n");
    } else if (sscanf(command, "AT+CIPSEND=%u", &n) == 1) {
        reading = n;
        text("\r\nOK\r\n>");
    } else if (!strncmp(command, "AT+CIPCLOSE", 11)) {
        text("\r\nERROR\r\n"); /* No previous connection. */
    } else {
        text("\r\nOK\r\n");
    }
    command_len = 0;
    return 1;
}

static void reset(int variant, unsigned int count)
{
    head = tail = ticks = calls = sent = queries = reads = 0;
    wire_length = 0;
    mode = variant;
    device_available = count;
    overflow = stall = reading = command_len = 0;
    esp_error = esp_closed = 0;
}

int main(void)
{
    unsigned int i;
    unsigned char data[ESP_CHUNK];
    for (i = 0; i < sizeof(fixture); ++i) fixture[i] = i;
    memcpy(fixture + 16, "\r\nERROR\r\nOK\r\n+IPD,\0>", sizeof("\r\nERROR\r\nOK\r\n+IPD,\0>"));
    for (i = 0; i < 2; ++i) {
        reset(i, ESP_CHUNK + 88);
        assert(esp_recv(data) == ESP_CHUNK);
        assert(!memcmp(data, fixture, ESP_CHUNK));
        assert(esp_recv(data) == 88);
        assert(!memcmp(data, fixture, 88));
        assert(esp_recv(data) == 0);
        assert(!esp_error && reads == 2 && queries == 3 && head == tail);
    }
    reset(2, ESP_CHUNK + 88);
    assert(esp_recv(data) == ESP_CHUNK && esp_closed);
    assert(esp_recv(data) == 88);
    assert(esp_recv(data) == 0 && !esp_error);
    reset(3, ESP_CHUNK);
    assert(esp_recv(data) == -1 && esp_error == ESP_FRAMING);
    for (i = 5; i <= 7; ++i) {
        reset(i, 0);
        assert(esp_recv(data) == -1);
        assert(esp_error == (i == 7 ? ESP_AT_ERROR : ESP_FRAMING));
    }
    reset(0, 0); overflow = 1;
    assert(esp_recv(data) == -1 && esp_error == ESP_UART_ERROR);
    reset(0, 0); stall = 1;
    assert(esp_recv(data) == -1 && esp_error == ESP_TIMEOUT);
    reset(0, 0); ticks = 65530UL * 16; stall = 1;
    assert(esp_recv(data) == -1 && esp_error == ESP_TIMEOUT);
    reset(0, 0);
    assert(esp_open("IRATA.ONLINE") && !esp_error);
    assert(esp_send(fixture, 256) && sent == 256 && head == tail);
    reset(0, 0);
    assert(esp_join("A,\"B", "") && !esp_error);
    assert(!strcmp(command, "AT+CWJAP_CUR=\"A\\,\\\"B\",\"\"\r\n"));
    puts("ESP tests passed: binary framing, both headers, close/drain, bounds, errors, timeout/wrap, send, credentials.");
    return 0;
}
