#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

// Server configuration
typedef struct {
    int port;
    char *tmux_session;
    char *bind_addr;
} server_config_t;

// Start the server (blocks)
int server_start(server_config_t *config);

// Stop the server
void server_stop(void);

#endif
