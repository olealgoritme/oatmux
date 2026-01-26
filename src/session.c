#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

// ANSI escape codes
#define CLEAR_SCREEN    "\033[2J"
#define CURSOR_HOME     "\033[H"
#define CURSOR_HIDE     "\033[?25l"
#define CURSOR_SHOW     "\033[?25h"
#define BOLD            "\033[1m"
#define DIM             "\033[2m"
#define RESET           "\033[0m"
#define FG_CYAN         "\033[36m"
#define FG_GREEN        "\033[32m"
#define FG_YELLOW       "\033[33m"
#define FG_WHITE        "\033[37m"
#define BG_BLUE         "\033[44m"
#define CLEAR_LINE      "\033[2K"

int session_list_get(session_list_t *list) {
    list->count = 0;

    FILE *fp = popen("tmux list-sessions -F '#{session_name}|#{session_windows}|#{session_attached}|#{session_created}' 2>/dev/null", "r");
    if (!fp) {
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp) && list->count < MAX_SESSIONS) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        tmux_session_t *s = &list->sessions[list->count];

        // Parse: name|windows|attached|created
        char *token = strtok(line, "|");
        if (token) {
            strncpy(s->name, token, MAX_SESSION_NAME - 1);
            s->name[MAX_SESSION_NAME - 1] = '\0';
        }

        token = strtok(NULL, "|");
        if (token) s->windows = atoi(token);

        token = strtok(NULL, "|");
        if (token) s->attached = atoi(token);

        token = strtok(NULL, "|");
        if (token) {
            strncpy(s->created, token, sizeof(s->created) - 1);
            s->created[sizeof(s->created) - 1] = '\0';
        }

        list->count++;
    }

    pclose(fp);
    return list->count > 0 ? 0 : -1;
}

static struct termios orig_termios;
static int raw_mode_enabled = 0;

static void disable_raw_mode(void) {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        printf(CURSOR_SHOW);
        raw_mode_enabled = 0;
    }
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw_mode_enabled = 1;
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf(CURSOR_HIDE);
}

static int get_terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return 80;
    }
    return ws.ws_col;
}

static void draw_box_top(int width) {
    printf(FG_CYAN "╭");
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("╮" RESET "\n");
}

static void draw_box_bottom(int width) {
    printf(FG_CYAN "╰");
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("╯" RESET "\n");
}

static void draw_separator(int width) {
    printf(FG_CYAN "├");
    for (int i = 0; i < width - 2; i++) printf("─");
    printf("┤" RESET "\n");
}

static void render_ui(session_list_t *list, int selected) {
    int term_width = get_terminal_width();
    int box_width = term_width > 60 ? 56 : term_width - 4;

    printf(CLEAR_SCREEN CURSOR_HOME);
    printf("\n");

    // Header
    draw_box_top(box_width);
    printf(FG_CYAN "│" RESET BOLD "   🌾 oatmux - tmux session selector" RESET);
    int padding = box_width - 39;
    for (int i = 0; i < padding; i++) printf(" ");
    printf(FG_CYAN "│" RESET "\n");
    draw_separator(box_width);

    if (list->count == 0) {
        printf(FG_CYAN "│" RESET "  No tmux sessions found.                    ");
        padding = box_width - 46;
        for (int i = 0; i < padding; i++) printf(" ");
        printf(FG_CYAN "│" RESET "\n");
        printf(FG_CYAN "│" RESET "  Run: " FG_GREEN "tmux new -s <name>" RESET " to create one.  ");
        padding = box_width - 46;
        for (int i = 0; i < padding; i++) printf(" ");
        printf(FG_CYAN "│" RESET "\n");
    } else {
        for (int i = 0; i < list->count && i < 15; i++) {
            tmux_session_t *s = &list->sessions[i];

            printf(FG_CYAN "│" RESET);

            if (i == selected) {
                printf(BG_BLUE BOLD FG_WHITE " ▶ ");
            } else {
                printf("   ");
            }

            // Session name
            printf("%-20.20s", s->name);

            // Windows count
            printf(DIM " %2d win" RESET, s->windows);

            // Attached indicator
            if (s->attached) {
                printf(FG_GREEN " ●" RESET);
            } else {
                printf(DIM " ○" RESET);
            }

            if (i == selected) {
                printf(RESET);
            }

            // Padding
            int content_len = 3 + 20 + 7 + 2;
            padding = box_width - content_len - 2;
            for (int j = 0; j < padding; j++) printf(" ");

            printf(FG_CYAN "│" RESET "\n");
        }

        if (list->count > 15) {
            printf(FG_CYAN "│" RESET DIM "   ... and %d more" RESET, list->count - 15);
            padding = box_width - 22;
            for (int i = 0; i < padding; i++) printf(" ");
            printf(FG_CYAN "│" RESET "\n");
        }
    }

    draw_separator(box_width);

    // Footer
    printf(FG_CYAN "│" RESET DIM "  ↑↓ navigate  " RESET DIM "Enter select  " RESET DIM "q quit" RESET);
    padding = box_width - 42;
    for (int i = 0; i < padding; i++) printf(" ");
    printf(FG_CYAN "│" RESET "\n");

    draw_box_bottom(box_width);

    fflush(stdout);
}

char *session_select_interactive(void) {
    session_list_t list;

    if (session_list_get(&list) < 0 || list.count == 0) {
        printf("\n" FG_YELLOW "  No tmux sessions found." RESET "\n");
        printf("  Create one with: " FG_GREEN "tmux new -s <name>" RESET "\n\n");
        return NULL;
    }

    // If only one session, auto-select it
    if (list.count == 1) {
        printf("\n" FG_GREEN "  Auto-selecting session: %s" RESET "\n\n", list.sessions[0].name);
        return strdup(list.sessions[0].name);
    }

    enable_raw_mode();

    int selected = 0;
    int max_visible = list.count < 15 ? list.count : 15;

    render_ui(&list, selected);

    while (1) {
        int c = getchar();

        if (c == 'q' || c == 'Q' || c == 27) {
            // Check for escape sequence (arrow keys)
            if (c == 27) {
                int next = getchar();
                if (next == '[') {
                    int arrow = getchar();
                    if (arrow == 'A') { // Up
                        selected = (selected - 1 + max_visible) % max_visible;
                        render_ui(&list, selected);
                        continue;
                    } else if (arrow == 'B') { // Down
                        selected = (selected + 1) % max_visible;
                        render_ui(&list, selected);
                        continue;
                    }
                }
                // Plain escape - quit
                disable_raw_mode();
                printf(CLEAR_SCREEN CURSOR_HOME);
                printf("Cancelled.\n");
                return NULL;
            }
            // q pressed
            disable_raw_mode();
            printf(CLEAR_SCREEN CURSOR_HOME);
            printf("Cancelled.\n");
            return NULL;
        }

        if (c == '\n' || c == '\r') {
            disable_raw_mode();
            printf(CLEAR_SCREEN CURSOR_HOME);
            return strdup(list.sessions[selected].name);
        }

        if (c == 'k' || c == 'K') { // vim up
            selected = (selected - 1 + max_visible) % max_visible;
            render_ui(&list, selected);
        }

        if (c == 'j' || c == 'J') { // vim down
            selected = (selected + 1) % max_visible;
            render_ui(&list, selected);
        }

        // Number selection (1-9)
        if (c >= '1' && c <= '9') {
            int idx = c - '1';
            if (idx < list.count) {
                disable_raw_mode();
                printf(CLEAR_SCREEN CURSOR_HOME);
                return strdup(list.sessions[idx].name);
            }
        }
    }
}
