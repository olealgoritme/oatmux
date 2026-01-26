#ifndef TERMINAL_H
#define TERMINAL_H

#include <sys/types.h>

// Terminal session structure
typedef struct {
    pid_t pid;          // Child process PID
    int master_fd;      // PTY master file descriptor
    char *session_name; // tmux session name
    int running;        // Is the session running
} terminal_t;

// Create a new terminal attached to a tmux session
// Returns 0 on success, -1 on error
int terminal_create(terminal_t *term, const char *session_name);

// Read from terminal (non-blocking if no data)
// Returns bytes read, 0 if no data, -1 on error/closed
ssize_t terminal_read(terminal_t *term, char *buf, size_t bufsize);

// Write to terminal
// Returns bytes written, -1 on error
ssize_t terminal_write(terminal_t *term, const char *buf, size_t len);

// Resize terminal
int terminal_resize(terminal_t *term, int cols, int rows);

// Close terminal and cleanup
void terminal_close(terminal_t *term);

// Check if terminal is still running
int terminal_is_running(terminal_t *term);

#endif
