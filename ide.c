#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define IDE_VERSION "0.0.1"
#define CONTROL_KEY(c) ((c)&0x1f)

enum EditorKeys { ARROW_UP = 1000, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT, PAGE_UP, PAGE_DOWN, HOME_KEY, END_KEY, DELETE_KEY };

typedef struct TextRow TextRow;
typedef struct EditorState EditorState;
typedef struct AppendBuffer AppendBuffer;

struct TextRow {
    char *b;
    size_t len;
};

struct EditorState {
    struct termios original_termios;
    int x;
    int y;
    int columns;
    int rows;
    int text_rows;
    TextRow text;
};

struct AppendBuffer {
    char *b;
    size_t len;
};

EditorState editor;

void append_string(AppendBuffer *ab, const char *s, size_t len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) {
        return;
    }
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void free_append_buffer(AppendBuffer *ab) { free(ab->b); }

void clear_screen() { write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7); }

void panic(const char *message) {
    clear_screen();
    perror(message);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.original_termios) == -1) {
        panic("tcsetattr");
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &editor.original_termios) == -1) {
        panic("tcgetattr");
    }
    atexit(disable_raw_mode);
    struct termios raw = editor.original_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~OPOST;
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        panic("tcsetattr");
    }
}

int read_key() {
    do {
        char c;
        int e = read(STDIN_FILENO, &c, 1);
        if (e == 1) {
            if (c == '\x1b') {
                char sequence[3];
                if (read(STDIN_FILENO, &sequence[0], 1) != 1) {
                    return '\x1b';
                }
                if (read(STDIN_FILENO, &sequence[1], 1) != 1) {
                    return '\x1b';
                }
                if (sequence[0] == '[') {
                    if (sequence[1] >= '0' && sequence[1] <= '9') {
                        if (read(STDIN_FILENO, &sequence[2], 1) != 1) {
                            return '\x1b';
                        }
                        if (sequence[2] == '~') {
                            switch (sequence[1]) {
                            case '1': return HOME_KEY;
                            case '3': return DELETE_KEY;
                            case '4': return END_KEY;
                            case '5': return PAGE_UP;
                            case '6': return PAGE_DOWN;
                            case '7': return HOME_KEY;
                            case '8': return END_KEY;
                            }
                        }
                    } else {
                        switch (sequence[1]) {
                        case 'A': return ARROW_UP;
                        case 'B': return ARROW_DOWN;
                        case 'D': return ARROW_LEFT;
                        case 'C': return ARROW_RIGHT;
                        case 'H': return HOME_KEY;
                        case 'F': return END_KEY;
                        }
                    }
                } else if (sequence[0] == 'O') {
                    switch (sequence[1]) {
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
                return '\x1b';
            } else {
                return c;
            }
        } else if (e == -1 && errno != EAGAIN) {
            panic("read");
        }
    } while (true);
}

void draw_rows(AppendBuffer *ab) {
    int rows = editor.rows - 1;
    for (int r = 0; r < rows; r++) {
        if (r >= editor.text_rows) {
            append_string(ab, "~\x1b[K\r\n", 1 + 3 + 2);
        } else {
            int len = editor.text.len;
            if (len > editor.columns) {
                len = editor.columns;
            }
            append_string(ab, editor.text.b, len);
        }
    }
    append_string(ab, "~\x1b[K", 1 + 3);

    char welcome[80];
    int welcome_len = snprintf(welcome, sizeof(welcome), "ide -- version %s", IDE_VERSION);
    if (welcome_len > editor.columns) {
        welcome_len = editor.columns;
    }
    int padding = (editor.columns - welcome_len) / 2;
    if (padding > 0) {
        append_string(ab, "~", 1);
        padding--;
    }
    while (padding > 0) {
        append_string(ab, " ", 1);
        padding--;
    }
    append_string(ab, welcome, welcome_len);
}

void draw_screen() {
    AppendBuffer ab = {NULL, 0};
    append_string(&ab, "\x1b[?25l\x1b[H", 6 + 3);
    draw_rows(&ab);
    char characters[32];
    snprintf(characters, sizeof(characters), "\x1b[%d;%dH", editor.y + 1, editor.x + 1);
    append_string(&ab, characters, strlen(characters));
    append_string(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    free_append_buffer(&ab);
}

void process_keypress() {
    int c = read_key();
    switch (c) {
    case CONTROL_KEY('q'):
        clear_screen();
        exit(0);
        break;
    case ARROW_UP:
        if (editor.y > 0) {
            editor.y--;
        }
        break;
    case ARROW_DOWN:
        if (editor.y < editor.rows - 1) {
            editor.y++;
        }
        break;
    case ARROW_LEFT:
        if (editor.x > 0) {
            editor.x--;
        }
        break;
    case ARROW_RIGHT:
        if (editor.x < editor.columns - 1) {
            editor.x++;
        }
        break;
    case PAGE_UP:
        editor.y -= editor.rows;
        if (editor.y < 0) {
            editor.y = 0;
        }
        break;
    case PAGE_DOWN:
        editor.y += editor.rows;
        if (editor.y >= editor.rows) {
            editor.y = editor.rows - 1;
        }
        break;
    case HOME_KEY: editor.x = 0; break;
    case END_KEY: editor.x = editor.columns - 1; break;
    default:
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else if (c != '\0') {
            printf("%d (%c)\r\n", c, c);
        }
    }
}

int get_cursor_position(int *columns, int *rows) {
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }
    unsigned int i = 0;
    char characters[32];
    while (i < sizeof(characters) - 1) {
        if (read(STDIN_FILENO, &characters[i], 1) != 1) {
            break;
        }
        if (characters[i] == 'R') {
            break;
        }
        i++;
    }
    characters[i] = '\0';
    if (characters[0] != '\x1b' || characters[1] != '[') {
        return -1;
    }
    if (sscanf(&characters[2], "%d;%d", rows, columns) != 2) {
        return -1;
    }
    return 0;
}

int get_window_size(int *columns, int *rows) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
        return -1;
    } else {
        return get_cursor_position(columns, rows);
    }
}

void init_edit() {
    if (get_window_size(&editor.columns, &editor.rows) == -1) {
        panic("get_window_size");
    }
    editor.x = 0;
    editor.y = 0;
    editor.text_rows = 0;
}

void edit_open() {
    char *line = "Hello world!";
    ssize_t len = 13;
    editor.text.len = 13;
    editor.text.b = malloc(len + 1);
    memcpy(editor.text.b, line, len);
    editor.text.b[len] = '\0';
    editor.text_rows = 1;
}

int main() {
    enable_raw_mode();
    init_edit();
    edit_open();
    while (true) {
        draw_screen();
        process_keypress();
    }
    return 0;
}
