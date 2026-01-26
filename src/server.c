#include "server.h"
#include "websocket.h"
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 65536

// Embedded HTML page with xterm.js
static const char *HTML_PAGE =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"    <meta charset=\"UTF-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\">\n"
"    <title>tmux Web Terminal</title>\n"
"    <link rel=\"stylesheet\" href=\"https://cdn.jsdelivr.net/npm/xterm@5.3.0/css/xterm.css\">\n"
"    <style>\n"
"        * { margin: 0; padding: 0; box-sizing: border-box; }\n"
"        html, body { height: 100%%; width: 100%%; background: #000; overflow: hidden; }\n"
"        #terminal { height: 100%%; width: 100%%; }\n"
"        #status { position: fixed; top: 5px; right: 10px; color: #0f0; font-family: monospace; font-size: 12px; z-index: 1000; }\n"
"        .disconnected { color: #f00 !important; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div id=\"status\">Connecting...</div>\n"
"    <div id=\"terminal\"></div>\n"
"    <script src=\"https://cdn.jsdelivr.net/npm/xterm@5.3.0/lib/xterm.min.js\"></script>\n"
"    <script src=\"https://cdn.jsdelivr.net/npm/xterm-addon-fit@0.8.0/lib/xterm-addon-fit.min.js\"></script>\n"
"    <script src=\"https://cdn.jsdelivr.net/npm/xterm-addon-web-links@0.9.0/lib/xterm-addon-web-links.min.js\"></script>\n"
"    <script>\n"
"        const term = new Terminal({\n"
"            cursorBlink: true,\n"
"            fontSize: 14,\n"
"            fontFamily: 'Menlo, Monaco, \"Courier New\", monospace',\n"
"            theme: { background: '#000000' },\n"
"            scrollback: 10000\n"
"        });\n"
"        const fitAddon = new FitAddon.FitAddon();\n"
"        const webLinksAddon = new WebLinksAddon.WebLinksAddon();\n"
"        term.loadAddon(fitAddon);\n"
"        term.loadAddon(webLinksAddon);\n"
"        term.open(document.getElementById('terminal'));\n"
"        fitAddon.fit();\n"
"\n"
"        const status = document.getElementById('status');\n"
"        let ws;\n"
"        let reconnectTimer;\n"
"\n"
"        function connect() {\n"
"            const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';\n"
"            ws = new WebSocket(protocol + '//' + location.host + '/ws');\n"
"            ws.binaryType = 'arraybuffer';\n"
"\n"
"            ws.onopen = () => {\n"
"                status.textContent = 'Connected';\n"
"                status.classList.remove('disconnected');\n"
"                // Send initial size\n"
"                const size = { type: 'resize', cols: term.cols, rows: term.rows };\n"
"                ws.send(JSON.stringify(size));\n"
"            };\n"
"\n"
"            ws.onmessage = (event) => {\n"
"                if (event.data instanceof ArrayBuffer) {\n"
"                    term.write(new Uint8Array(event.data));\n"
"                } else {\n"
"                    term.write(event.data);\n"
"                }\n"
"            };\n"
"\n"
"            ws.onclose = () => {\n"
"                status.textContent = 'Disconnected - Reconnecting...';\n"
"                status.classList.add('disconnected');\n"
"                reconnectTimer = setTimeout(connect, 2000);\n"
"            };\n"
"\n"
"            ws.onerror = (err) => {\n"
"                console.error('WebSocket error:', err);\n"
"                ws.close();\n"
"            };\n"
"        }\n"
"\n"
"        term.onData((data) => {\n"
"            if (ws && ws.readyState === WebSocket.OPEN) {\n"
"                ws.send(data);\n"
"            }\n"
"        });\n"
"\n"
"        window.addEventListener('resize', () => {\n"
"            fitAddon.fit();\n"
"            if (ws && ws.readyState === WebSocket.OPEN) {\n"
"                const size = { type: 'resize', cols: term.cols, rows: term.rows };\n"
"                ws.send(JSON.stringify(size));\n"
"            }\n"
"        });\n"
"\n"
"        // Handle mobile keyboard\n"
"        term.textarea.setAttribute('autocapitalize', 'off');\n"
"        term.textarea.setAttribute('autocorrect', 'off');\n"
"\n"
"        connect();\n"
"    </script>\n"
"</body>\n"
"</html>\n";

static volatile int server_running = 1;
static int server_socket = -1;

// Client connection state
typedef struct {
    int socket_fd;
    terminal_t terminal;
    int websocket_ready;
    pthread_t thread;
    char *session_name;
} client_t;

static void signal_handler(int sig) {
    (void)sig;
    server_running = 0;
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
}

// Parse HTTP headers and extract WebSocket key
static int parse_http_request(const char *request, char *ws_key, size_t ws_key_size, char *path, size_t path_size) {
    // Extract path
    const char *path_start = strchr(request, ' ');
    if (!path_start) return -1;
    path_start++;

    const char *path_end = strchr(path_start, ' ');
    if (!path_end) return -1;

    size_t path_len = path_end - path_start;
    if (path_len >= path_size) path_len = path_size - 1;
    strncpy(path, path_start, path_len);
    path[path_len] = '\0';

    // Find Sec-WebSocket-Key header
    const char *key_header = strstr(request, "Sec-WebSocket-Key: ");
    if (!key_header) {
        ws_key[0] = '\0';
        return 0; // Regular HTTP request
    }

    key_header += 19; // Skip header name
    const char *key_end = strstr(key_header, "\r\n");
    if (!key_end) return -1;

    size_t key_len = key_end - key_header;
    if (key_len >= ws_key_size) return -1;

    strncpy(ws_key, key_header, key_len);
    ws_key[key_len] = '\0';

    return 1; // WebSocket upgrade request
}

// Send HTTP response
static int send_http_response(int fd, int status_code, const char *status_text,
                              const char *content_type, const char *body, size_t body_len) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_len);

    if (write(fd, header, header_len) < 0) return -1;
    if (body_len > 0 && write(fd, body, body_len) < 0) return -1;
    return 0;
}

// Send WebSocket upgrade response
static int send_ws_upgrade_response(int fd, const char *accept_key) {
    char response[512];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    return write(fd, response, len) == len ? 0 : -1;
}

// Client handler thread
static void *client_handler(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[BUFFER_SIZE];
    uint8_t ws_buffer[BUFFER_SIZE];
    size_t ws_buffer_len = 0;

    // Read HTTP request
    ssize_t n = read(client->socket_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        goto cleanup;
    }
    buffer[n] = '\0';

    char ws_key[256] = {0};
    char path[256] = {0};
    int is_websocket = parse_http_request(buffer, ws_key, sizeof(ws_key), path, sizeof(path));

    if (is_websocket < 0) {
        goto cleanup;
    }

    // Handle regular HTTP request
    if (is_websocket == 0 || strlen(ws_key) == 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            send_http_response(client->socket_fd, 200, "OK", "text/html", HTML_PAGE, strlen(HTML_PAGE));
        } else {
            const char *not_found = "404 Not Found";
            send_http_response(client->socket_fd, 404, "Not Found", "text/plain", not_found, strlen(not_found));
        }
        goto cleanup;
    }

    // WebSocket upgrade
    char accept_key[64];
    if (ws_generate_accept_key(ws_key, accept_key, sizeof(accept_key)) < 0) {
        goto cleanup;
    }

    if (send_ws_upgrade_response(client->socket_fd, accept_key) < 0) {
        goto cleanup;
    }

    client->websocket_ready = 1;

    // Create terminal attached to tmux session
    if (terminal_create(&client->terminal, client->session_name) < 0) {
        const char *error = "Failed to attach to tmux session";
        ws_send_text(client->socket_fd, error, strlen(error));
        goto cleanup;
    }

    // Main I/O loop
    fd_set read_fds;
    struct timeval tv;

    while (server_running && terminal_is_running(&client->terminal)) {
        FD_ZERO(&read_fds);
        FD_SET(client->socket_fd, &read_fds);
        FD_SET(client->terminal.master_fd, &read_fds);

        int max_fd = client->socket_fd > client->terminal.master_fd ?
                     client->socket_fd : client->terminal.master_fd;

        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50ms timeout

        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // Read from WebSocket client
        if (FD_ISSET(client->socket_fd, &read_fds)) {
            n = read(client->socket_fd, ws_buffer + ws_buffer_len,
                     sizeof(ws_buffer) - ws_buffer_len);
            if (n <= 0) break;
            ws_buffer_len += n;

            // Process WebSocket frames
            while (ws_buffer_len > 0) {
                ws_frame_t frame;
                size_t consumed;

                if (ws_parse_frame(ws_buffer, ws_buffer_len, &frame, &consumed) < 0) {
                    break; // Need more data
                }

                // Handle frame based on opcode
                switch (frame.opcode) {
                    case WS_OPCODE_TEXT:
                    case WS_OPCODE_BIN:
                        // Check for resize command
                        if (frame.payload_len > 0 && frame.payload[0] == '{') {
                            // Try to parse as JSON resize command
                            int cols, rows;
                            if (sscanf((char *)frame.payload,
                                       "{\"type\":\"resize\",\"cols\":%d,\"rows\":%d}",
                                       &cols, &rows) == 2) {
                                terminal_resize(&client->terminal, cols, rows);
                            } else {
                                // Regular input
                                terminal_write(&client->terminal,
                                             (char *)frame.payload, frame.payload_len);
                            }
                        } else if (frame.payload_len > 0) {
                            terminal_write(&client->terminal,
                                         (char *)frame.payload, frame.payload_len);
                        }
                        break;

                    case WS_OPCODE_PING:
                        ws_send_pong(client->socket_fd, frame.payload, frame.payload_len);
                        break;

                    case WS_OPCODE_CLOSE:
                        ws_send_close(client->socket_fd);
                        free(frame.payload);
                        goto cleanup;
                }

                free(frame.payload);

                // Remove processed data from buffer
                memmove(ws_buffer, ws_buffer + consumed, ws_buffer_len - consumed);
                ws_buffer_len -= consumed;
            }
        }

        // Read from terminal
        if (FD_ISSET(client->terminal.master_fd, &read_fds)) {
            n = terminal_read(&client->terminal, buffer, sizeof(buffer));
            if (n > 0) {
                ws_send_binary(client->socket_fd, (uint8_t *)buffer, n);
            } else if (n < 0) {
                break; // Terminal closed
            }
        }
    }

cleanup:
    terminal_close(&client->terminal);
    close(client->socket_fd);
    free(client->session_name);
    free(client);
    return NULL;
}

int server_start(server_config_t *config) {
    struct sockaddr_in server_addr;

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return -1;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port);

    if (config->bind_addr && strlen(config->bind_addr) > 0) {
        inet_pton(AF_INET, config->bind_addr, &server_addr.sin_addr);
    } else {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        return -1;
    }

    // Listen
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_socket);
        return -1;
    }

    printf("Server started on http://%s:%d\n",
           config->bind_addr ? config->bind_addr : "0.0.0.0",
           config->port);
    printf("tmux session: %s\n", config->tmux_session);
    printf("Press Ctrl+C to stop\n\n");

    // Accept loop
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR || !server_running) break;
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // Create client structure
        client_t *client = calloc(1, sizeof(client_t));
        if (!client) {
            close(client_fd);
            continue;
        }

        client->socket_fd = client_fd;
        client->session_name = strdup(config->tmux_session);
        client->websocket_ready = 0;

        // Spawn handler thread
        if (pthread_create(&client->thread, NULL, client_handler, client) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(client->session_name);
            free(client);
            continue;
        }

        pthread_detach(client->thread);
    }

    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }

    printf("\nServer stopped\n");
    return 0;
}

void server_stop(void) {
    server_running = 0;
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
}
