#ifndef SESSION_H
#define SESSION_H

#define MAX_SESSIONS 64
#define MAX_SESSION_NAME 256

typedef struct {
    char name[MAX_SESSION_NAME];
    int windows;
    int attached;
    char created[32];
} tmux_session_t;

typedef struct {
    tmux_session_t sessions[MAX_SESSIONS];
    int count;
} session_list_t;

// Get list of tmux sessions
// Returns 0 on success, -1 on error
int session_list_get(session_list_t *list);

// Interactive session selector
// Returns selected session name (must be freed) or NULL on cancel/error
char *session_select_interactive(void);

#endif
