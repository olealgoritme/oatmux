#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "server.h"

#define DEFAULT_PORT 8080
#define DEFAULT_SESSION "0"

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Stream a tmux session to a web browser\n\n");
    printf("Options:\n");
    printf("  -p, --port PORT        Port to listen on (default: %d)\n", DEFAULT_PORT);
    printf("  -s, --session NAME     tmux session name (default: %s)\n", DEFAULT_SESSION);
    printf("  -b, --bind ADDR        Address to bind to (default: 0.0.0.0)\n");
    printf("  -h, --help             Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s -s mysession           # Attach to 'mysession' on port 8080\n", program_name);
    printf("  %s -p 3000 -s dev         # Attach to 'dev' on port 3000\n", program_name);
    printf("  %s -b 127.0.0.1 -p 8080   # Only allow local connections\n", program_name);
    printf("\nTo list available tmux sessions:\n");
    printf("  tmux list-sessions\n");
}

int main(int argc, char *argv[]) {
    server_config_t config = {
        .port = DEFAULT_PORT,
        .tmux_session = DEFAULT_SESSION,
        .bind_addr = NULL
    };

    static struct option long_options[] = {
        {"port",    required_argument, 0, 'p'},
        {"session", required_argument, 0, 's'},
        {"bind",    required_argument, 0, 'b'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:s:b:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                config.port = atoi(optarg);
                if (config.port <= 0 || config.port > 65535) {
                    fprintf(stderr, "Error: Invalid port number '%s'\n", optarg);
                    return 1;
                }
                break;
            case 's':
                config.tmux_session = optarg;
                break;
            case 'b':
                config.bind_addr = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    printf("tmux Web Terminal\n");
    printf("=================\n\n");

    return server_start(&config);
}
