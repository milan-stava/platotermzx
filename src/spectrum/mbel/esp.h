#ifndef MBEL_ESP_H
#define MBEL_ESP_H

#define ESP_CHUNK 512
#define ESP_TIMEOUT 1
#define ESP_AT_ERROR 2
#define ESP_FRAMING 3
#define ESP_UART_ERROR 4

extern unsigned char esp_error;
extern unsigned char esp_closed;

int esp_open(const char *host);
void esp_close(void);
int esp_join(const char *ssid, const char *password);
int esp_send(const unsigned char *data, unsigned int count);
/* Returns payload length, 0 if idle/end, -1 on error. */
int esp_recv(unsigned char *data);

#endif
