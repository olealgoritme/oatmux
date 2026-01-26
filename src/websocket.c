#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/sha.h>

// WebSocket magic GUID for handshake
static const char *WS_MAGIC_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Base64 encoding table
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 encode
static int base64_encode(const unsigned char *input, size_t input_len, char *output, size_t output_size) {
    size_t i, j;
    size_t encoded_len = 4 * ((input_len + 2) / 3);

    if (output_size < encoded_len + 1) {
        return -1;
    }

    for (i = 0, j = 0; i < input_len;) {
        uint32_t octet_a = i < input_len ? input[i++] : 0;
        uint32_t octet_b = i < input_len ? input[i++] : 0;
        uint32_t octet_c = i < input_len ? input[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[j++] = b64_table[(triple >> 18) & 0x3F];
        output[j++] = b64_table[(triple >> 12) & 0x3F];
        output[j++] = b64_table[(triple >> 6) & 0x3F];
        output[j++] = b64_table[triple & 0x3F];
    }

    // Add padding
    size_t mod = input_len % 3;
    if (mod == 1) {
        output[encoded_len - 2] = '=';
        output[encoded_len - 1] = '=';
    } else if (mod == 2) {
        output[encoded_len - 1] = '=';
    }

    output[encoded_len] = '\0';
    return 0;
}

int ws_generate_accept_key(const char *client_key, char *accept_key, size_t accept_key_size) {
    char combined[256];
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];

    // Combine client key with magic GUID
    snprintf(combined, sizeof(combined), "%s%s", client_key, WS_MAGIC_GUID);

    // SHA1 hash
    SHA1((unsigned char *)combined, strlen(combined), sha1_hash);

    // Base64 encode
    return base64_encode(sha1_hash, SHA_DIGEST_LENGTH, accept_key, accept_key_size);
}

int ws_parse_frame(const uint8_t *data, size_t data_len, ws_frame_t *frame, size_t *consumed) {
    if (data_len < 2) {
        return -1; // Need more data
    }

    size_t offset = 0;

    // First byte: FIN + RSV + Opcode
    uint8_t first_byte = data[offset++];
    frame->opcode = first_byte & 0x0F;

    // Second byte: MASK + Payload length
    uint8_t second_byte = data[offset++];
    int masked = (second_byte & 0x80) != 0;
    size_t payload_len = second_byte & 0x7F;

    // Extended payload length
    if (payload_len == 126) {
        if (data_len < offset + 2) return -1;
        payload_len = (data[offset] << 8) | data[offset + 1];
        offset += 2;
    } else if (payload_len == 127) {
        if (data_len < offset + 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | data[offset + i];
        }
        offset += 8;
    }

    // Masking key (if present)
    uint8_t mask_key[4] = {0};
    if (masked) {
        if (data_len < offset + 4) return -1;
        memcpy(mask_key, data + offset, 4);
        offset += 4;
    }

    // Check if we have the full payload
    if (data_len < offset + payload_len) {
        return -1; // Need more data
    }

    // Allocate and copy/unmask payload
    frame->payload = malloc(payload_len + 1);
    if (!frame->payload) return -2;

    for (size_t i = 0; i < payload_len; i++) {
        if (masked) {
            frame->payload[i] = data[offset + i] ^ mask_key[i % 4];
        } else {
            frame->payload[i] = data[offset + i];
        }
    }
    frame->payload[payload_len] = '\0';
    frame->payload_len = payload_len;

    *consumed = offset + payload_len;
    return 0;
}

int ws_build_frame(uint8_t opcode, const uint8_t *payload, size_t payload_len,
                   uint8_t *out, size_t out_size, size_t *out_len) {
    size_t header_len;

    if (payload_len < 126) {
        header_len = 2;
    } else if (payload_len < 65536) {
        header_len = 4;
    } else {
        header_len = 10;
    }

    if (out_size < header_len + payload_len) {
        return -1;
    }

    size_t offset = 0;

    // FIN + Opcode
    out[offset++] = 0x80 | opcode;

    // Payload length (server->client is not masked)
    if (payload_len < 126) {
        out[offset++] = payload_len;
    } else if (payload_len < 65536) {
        out[offset++] = 126;
        out[offset++] = (payload_len >> 8) & 0xFF;
        out[offset++] = payload_len & 0xFF;
    } else {
        out[offset++] = 127;
        for (int i = 7; i >= 0; i--) {
            out[offset++] = (payload_len >> (8 * i)) & 0xFF;
        }
    }

    // Copy payload
    memcpy(out + offset, payload, payload_len);

    *out_len = offset + payload_len;
    return 0;
}

int ws_send_text(int fd, const char *text, size_t len) {
    uint8_t *frame = malloc(len + 14);
    if (!frame) return -1;

    size_t frame_len;
    int ret = ws_build_frame(WS_OPCODE_TEXT, (const uint8_t *)text, len, frame, len + 14, &frame_len);
    if (ret < 0) {
        free(frame);
        return ret;
    }

    ssize_t written = write(fd, frame, frame_len);
    free(frame);
    return (written == (ssize_t)frame_len) ? 0 : -1;
}

int ws_send_binary(int fd, const uint8_t *data, size_t len) {
    uint8_t *frame = malloc(len + 14);
    if (!frame) return -1;

    size_t frame_len;
    int ret = ws_build_frame(WS_OPCODE_BIN, data, len, frame, len + 14, &frame_len);
    if (ret < 0) {
        free(frame);
        return ret;
    }

    ssize_t written = write(fd, frame, frame_len);
    free(frame);
    return (written == (ssize_t)frame_len) ? 0 : -1;
}

int ws_send_close(int fd) {
    uint8_t frame[4];
    size_t frame_len;
    ws_build_frame(WS_OPCODE_CLOSE, NULL, 0, frame, sizeof(frame), &frame_len);
    return write(fd, frame, frame_len) > 0 ? 0 : -1;
}

int ws_send_pong(int fd, const uint8_t *data, size_t len) {
    uint8_t *frame = malloc(len + 14);
    if (!frame) return -1;

    size_t frame_len;
    int ret = ws_build_frame(WS_OPCODE_PONG, data, len, frame, len + 14, &frame_len);
    if (ret < 0) {
        free(frame);
        return ret;
    }

    ssize_t written = write(fd, frame, frame_len);
    free(frame);
    return (written == (ssize_t)frame_len) ? 0 : -1;
}
