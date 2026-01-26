#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdint.h>
#include <stddef.h>

// WebSocket opcodes
#define WS_OPCODE_CONT   0x00
#define WS_OPCODE_TEXT   0x01
#define WS_OPCODE_BIN    0x02
#define WS_OPCODE_CLOSE  0x08
#define WS_OPCODE_PING   0x09
#define WS_OPCODE_PONG   0x0A

// WebSocket frame structure
typedef struct {
    uint8_t opcode;
    uint8_t *payload;
    size_t payload_len;
} ws_frame_t;

// Generate WebSocket accept key from client key
int ws_generate_accept_key(const char *client_key, char *accept_key, size_t accept_key_size);

// Parse incoming WebSocket frame (handles masking)
int ws_parse_frame(const uint8_t *data, size_t data_len, ws_frame_t *frame, size_t *consumed);

// Build outgoing WebSocket frame (server frames are not masked)
int ws_build_frame(uint8_t opcode, const uint8_t *payload, size_t payload_len,
                   uint8_t *out, size_t out_size, size_t *out_len);

// Send a text frame
int ws_send_text(int fd, const char *text, size_t len);

// Send a binary frame
int ws_send_binary(int fd, const uint8_t *data, size_t len);

// Send close frame
int ws_send_close(int fd);

// Send pong frame
int ws_send_pong(int fd, const uint8_t *data, size_t len);

#endif
