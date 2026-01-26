#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <pty.h>
#include <termios.h>

int terminal_create(terminal_t *term, const char *session_name) {
    struct winsize ws = {
        .ws_row = 24,
        .ws_col = 80,
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };

    pid_t pid = forkpty(&term->master_fd, NULL, NULL, &ws);

    if (pid < 0) {
        perror("forkpty");
        return -1;
    }

    if (pid == 0) {
        // Child process - exec tmux attach
        char *args[5];
        args[0] = "tmux";
        args[1] = "attach-session";
        args[2] = "-t";
        args[3] = (char *)session_name;
        args[4] = NULL;

        // Set TERM environment variable
        setenv("TERM", "xterm-256color", 1);

        execvp("tmux", args);
        // If exec fails, try to create the session
        perror("execvp tmux attach");

        // Try creating a new session instead
        args[1] = "new-session";
        args[2] = "-s";
        execvp("tmux", args);
        perror("execvp tmux new");
        _exit(1);
    }

    // Parent process
    term->pid = pid;
    term->session_name = strdup(session_name);
    term->running = 1;

    // Set master fd to non-blocking
    int flags = fcntl(term->master_fd, F_GETFL, 0);
    fcntl(term->master_fd, F_SETFL, flags | O_NONBLOCK);

    return 0;
}

ssize_t terminal_read(terminal_t *term, char *buf, size_t bufsize) {
    if (!term->running) return -1;

    ssize_t n = read(term->master_fd, buf, bufsize);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data available
        }
        term->running = 0;
        return -1;
    }

    if (n == 0) {
        term->running = 0;
        return -1; // EOF
    }

    return n;
}

ssize_t terminal_write(terminal_t *term, const char *buf, size_t len) {
    if (!term->running) return -1;

    ssize_t total = 0;
    while ((size_t)total < len) {
        ssize_t n = write(term->master_fd, buf + total, len - total);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000); // Brief pause and retry
                continue;
            }
            return -1;
        }
        total += n;
    }

    return total;
}

int terminal_resize(terminal_t *term, int cols, int rows) {
    if (!term->running) return -1;

    struct winsize ws = {
        .ws_row = rows,
        .ws_col = cols,
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };

    return ioctl(term->master_fd, TIOCSWINSZ, &ws);
}

void terminal_close(terminal_t *term) {
    if (term->master_fd >= 0) {
        close(term->master_fd);
        term->master_fd = -1;
    }

    if (term->pid > 0) {
        kill(term->pid, SIGHUP);
        waitpid(term->pid, NULL, WNOHANG);
        term->pid = 0;
    }

    if (term->session_name) {
        free(term->session_name);
        term->session_name = NULL;
    }

    term->running = 0;
}

int terminal_is_running(terminal_t *term) {
    if (!term->running) return 0;

    // Check if child process is still alive
    int status;
    pid_t result = waitpid(term->pid, &status, WNOHANG);

    if (result == term->pid) {
        // Process has exited
        term->running = 0;
        return 0;
    }

    return 1;
}
