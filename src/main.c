#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "server.h"
#include "session.h"

#define DEFAULT_PORT 8080

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Stream a tmux session to a web browser.\n\n");
    printf("Options:\n");
    printf("  -p, --port PORT        Port to listen on (default: %d)\n", DEFAULT_PORT);
    printf("  -s, --session NAME     tmux session name (interactive if omitted)\n");
    printf("  -b, --bind ADDR        Address to bind to (default: 0.0.0.0)\n");
    printf("  -l, --list             List available tmux sessions\n");
    printf("  -h, --help             Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                        # Interactive session selector\n", program_name);
    printf("  %s -s mysession           # Attach to 'mysession' on port 8080\n", program_name);
    printf("  %s -p 3000 -s dev         # Attach to 'dev' on port 3000\n", program_name);
    printf("  %s -b 127.0.0.1           # Only allow local connections\n", program_name);
}

static void list_sessions(void) {
    session_list_t list;

    if (session_list_get(&list) < 0) {
        printf("No tmux sessions found.\n");
        printf("Create one with: tmux new -s <name>\n");
        return;
    }

    printf("\n");
    printf("  %-20s  %s  %s\n", "SESSION", "WINDOWS", "ATTACHED");
    printf("  %-20s  %s  %s\n", "───────", "───────", "────────");

    for (int i = 0; i < list.count; i++) {
        tmux_session_t *s = &list.sessions[i];
        printf("  %-20s  %7d  %s\n",
               s->name,
               s->windows,
               s->attached ? "yes" : "no");
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    server_config_t config = {
        .port = DEFAULT_PORT,
        .tmux_session = NULL,
        .bind_addr = NULL
    };

    char *allocated_session = NULL;

    static struct option long_options[] = {
        {"port",    required_argument, 0, 'p'},
        {"session", required_argument, 0, 's'},
        {"bind",    required_argument, 0, 'b'},
        {"list",    no_argument,       0, 'l'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:s:b:lh", long_options, NULL)) != -1) {
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
            case 'l':
                list_sessions();
                return 0;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // If no session specified, show interactive selector
    if (!config.tmux_session) {
        allocated_session = session_select_interactive();
        if (!allocated_session) {
            return 1;
        }
        config.tmux_session = allocated_session;
    }

    printf("\n");
    printf("  \033[1m🌾 oatmux\033[0m\n");
    printf("  ─────────────────────────────────\n");
    printf("  Session:  \033[32m%s\033[0m\n", config.tmux_session);
    printf("  URL:      \033[36mhttp://%s:%d\033[0m\n",
           config.bind_addr ? config.bind_addr : "0.0.0.0",
           config.port);
    printf("  ─────────────────────────────────\n");
    printf("  Press \033[1mCtrl+C\033[0m to stop\n");
    printf("\n");

    int result = server_start(&config);

    if (allocated_session) {
        free(allocated_session);
    }

    return result;
}
