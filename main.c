#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "terminal.h"
#include "buffer.h"
#include "append_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>

/*** Configuration Matrix ***/
typedef struct {
    int cx, cy;          // Logical cursor row/col mapping coordinates within file
    int rx, ry;          // Actual render transformations (tab expansion variants)
    int row_offset;      // Vertical viewport scrolling index tracker
    int col_offset;      // Horizontal viewport scrolling index tracker
    int screen_rows;     // Output terminal height boundary configuration
    int screen_cols;     // Output terminal width boundary configuration
    int num_rows;        // Total logical structural row indexes tracked
    EditorRow *row;      // Matrix map array slices
    int dirty;           // State mutation tracking status bit flag
    char *filename;      // File cache pointer identity path string
    char statusmsg[80];  // Output bottom log text block
    time_t statusmsg_time;
    GapBuffer buffer;    // Unified memory tracking engine
} EditorConfig;

EditorConfig E;

void editorSetStatusMessage(const char *fmt, ...);
void editorUpdateRows(void);

/*** System Architecture Allocations & Lifecycles ***/
void editorInit(void) {
    E.cx = 0; E.cy = 0; E.rx = 0; E.ry = 0;
    E.row_offset = 0; E.col_offset = 0;
    E.num_rows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    bufferInit(&E.buffer, 1024);

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        perror("FATAL: Hardware viewport capture initialization error");
        exit(1);
    }
    E.screen_rows -= 2; // Allocate space for Status Bar and Message Log Prompts
}

void editorCleanup(void) {
    if (E.filename) free(E.filename);
    if (E.row) {
        for (int i = 0; i < E.num_rows; i++) {
            if (E.row[i].chars) free(E.row[i].chars);
        }
        free(E.row);
    }
    bufferFree(&E.buffer);
}

/*** File Matrix Synchronizer ***/
void editorUpdateRows(void) {
    // Dynamic parsing sweep mapping logical text rows inside the flat Gap Buffer
    if (E.row) {
        for (int i = 0; i < E.num_rows; i++) {
            if (E.row[i].chars) free(E.row[i].chars);
        }
        free(E.row);
        E.row = NULL;
    }
    E.num_rows = 0;

    int total_len = 0;
    char *flat = bufferToString(&E.buffer, &total_len);
    if (!flat) return;

    int line_start = 0;
    for (int i = 0; i <= total_len; i++) {
        if (i == total_len || flat[i] == '\n') {
            E.row = realloc(E.row, sizeof(EditorRow) * (E.num_rows + 1));
            int line_len = i - line_start;
            E.row[E.num_rows].size = line_len;
            E.row[E.num_rows].chars_index = line_start;
            E.row[E.num_rows].chars = malloc(line_len + 1);
            memcpy(E.row[E.num_rows].chars, &flat[line_start], line_len);
            E.row[E.num_rows].chars[line_len] = '\0';
            E.num_rows++;
            line_start = i + 1;
        }
    }
    free(flat);
}

// Convert Row/Col mapping down to flat integer index array tracking inside Gap Buffer
int editorGetBufferIndex(int target_cx, int target_cy) {
    editorUpdateRows();
    if (target_cy >= E.num_rows) return bufferGetTotalSize(&E.buffer);
    int idx = E.row[target_cy].chars_index;
    int row_sz = E.row[target_cy].size;
    if (target_cx > row_sz) target_cx = row_sz;
    return idx + target_cx;
}

/*** File I/O Management ***/
void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        editorSetStatusMessage("Created target structural scratchfile: %s", filename);
        editorUpdateRows();
        return;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        for (int i = 0; i < linelen; i++) {
            bufferInsert(&E.buffer, line[i]);
        }
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
    editorUpdateRows();
}

void editorSave(void) {
    if (E.filename == NULL) {
        E.filename = "untitled.txt";
    }

    int len;
    char *buf = bufferToString(&E.buffer, &len);
    if (!buf) return;

    FILE *fp = fopen(E.filename, "w+");
    if (fp != NULL) {
        if (fwrite(buf, 1, len, fp) == len) {
            fclose(fp);
            free(buf);
            E.dirty = 0;
            editorSetStatusMessage("Disk Sync Complete. %d bytes written.", len);
            return;
        }
        fclose(fp);
    }
    free(buf);
    editorSetStatusMessage("FILE SERIALIZATION I/O SYSTEM ERROR: Write task rejected.");
}

/*** Real-time Interactive String Searching Layer ***/
void editorFindCallback(char *query, int key) {
    static int last_match = -1;
    static int direction = 1;

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    }

    if (key == ARROW_DOWN || key == ARROW_RIGHT) direction = 1;
    else if (key == ARROW_UP || key == ARROW_LEFT) direction = -1;
    else {
        last_match = -1;
        direction = 1;
    }

    if (query[0] == '\0') return;

    int total_len = 0;
    char *flat = bufferToString(&E.buffer, &total_len);
    int current_match = last_match;

    // Scan through structural text blocks
    for (int i = 0; i < E.num_rows; i++) {
        current_match = (current_match + direction + E.num_rows) % E.num_rows;
        EditorRow *row = &E.row[current_match];
        char *match = strstr(row->chars, query);
        if (match) {
            last_match = current_match;
            E.cy = current_match;
            E.cx = match - row->chars;
            E.row_offset = E.cy; // Instantly center viewport to frame target
            break;
        }
    }
    free(flat);
}

void editorFind(void) {
    int saved_cx = E.cx; int saved_cy = E.cy;
    int saved_col_offset = E.col_offset; int saved_row_offset = E.row_offset;

    // Allocate interactive prompt line inside message loop footer
    char query[32];
    query[0] = '\0';
    int query_len = 0;

    while (1) {
        editorSetStatusMessage("Search Term: %s (ESC to cancel | Enter to submit)", query);
        // Refresh terminal screen with current text and updated status messages
        void editorScroll(void);
        void editorRefreshScreen(void);
        editorScroll();
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY || c == BACKSPACE) {
            if (query_len > 0) query[--query_len] = '\0';
        } else if (c == '\x1b') {
            editorSetStatusMessage("");
            E.cx = saved_cx; E.cy = saved_cy;
            E.col_offset = saved_col_offset; E.row_offset = saved_row_offset;
            editorFindCallback(query, c);
            return;
        } else if (c == '\r') {
            editorSetStatusMessage("");
            editorFindCallback(query, c);
            return;
        } else if (c != '\0' && c < 1000) {
            if (query_len < (int)sizeof(query) - 1) {
                query[query_len++] = c;
                query[query_len] = '\0';
            }
        }
        editorFindCallback(query, c);
    }
}

/*** Viewport Scrolling Transformations ***/
void editorScroll(void) {
    E.rx = E.cx; // Base rendering coordinates
    
    // Bounds normalization step targeting vertical screen lines
    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + E.screen_rows) {
        E.row_offset = E.cy - E.screen_rows + 1;
    }
    // Bounds normalization targeting horizontal screen lines
    if (E.rx < E.col_offset) {
        E.col_offset = E.rx;
    }
    if (E.rx >= E.col_offset + E.screen_cols) {
        E.col_offset = E.rx - E.screen_cols + 1;
    }
}

/*** User Interface Render Sweeps ***/
void editorDrawRows(AppendBuffer *ab) {
    for (int y = 0; y < E.screen_rows; y++) {
        int file_row = y + E.row_offset;
        if (file_row >= E.num_rows) {
            // Render terminal line margins if processing empty context paths
            if (E.num_rows == 0 && y == E.screen_rows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "CoreEdit Core System Framework Engine v2.2026");
                if (welcomelen > E.screen_cols) welcomelen = E.screen_cols;
                int padding = (E.screen_cols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E.row[file_row].size - E.col_offset;
            if (len < 0) len = 0;
            if (len > E.screen_cols) len = E.screen_cols;
            abAppend(ab, &E.row[file_row].chars[E.col_offset], len);
        }

        abAppend(ab, "\x1b[K", 3); // Clear individual line terminal track cleanly
        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(AppendBuffer *ab) {
    abAppend(ab, "\x1b[7m", 4); // Activate inverse color formatting
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name Specified]", E.num_rows,
        E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.num_rows);
    if (len > E.screen_cols) len = E.screen_cols;
    abAppend(ab, status, len);
    while (len < E.screen_cols) {
        if (E.screen_cols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);  // Clear video mode inversion
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(AppendBuffer *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screen_cols) msglen = E.screen_cols;
    if (msglen && time(NULL) - E.statusmsg_time < 5) {
        abAppend(ab, E.statusmsg, msglen);
    }
}

void editorRefreshScreen(void) {
    editorUpdateRows();
    editorScroll();

    AppendBuffer ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // Hide systemic blinking hardware cursor temporarily
    abAppend(&ab, "\x1b[H", 3);    // Reposition coordinates home (1,1)

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    // Reposition physical cursor relative to internal viewport constraints
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.row_offset) + 1, (E.rx - E.col_offset) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); // Re-activate standard display cursor context tracking

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*** Input State Machine Handlers ***/
void editorMoveCursor(int key) {
    int row_sz = (E.cy < E.num_rows) ? E.row[E.cy].size : 0;

    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (E.cx < row_sz) {
                E.cx++;
            } else if (E.cy < E.num_rows - 1) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy > 0) E.cy--;
            break;
        case ARROW_DOWN:
            if (E.cy < E.num_rows - 1) E.cy++;
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = row_sz;
            break;
        case PAGE_UP:
            E.cy = E.row_offset;
            for (int i = 0; i < E.screen_rows; i++) if (E.cy > 0) E.cy--;
            break;
        case PAGE_DOWN:
            E.cy = E.row_offset + E.screen_rows - 1;
            if (E.cy >= E.num_rows) E.cy = E.num_rows - 1;
            for (int i = 0; i < E.screen_rows; i++) if (E.cy < E.num_rows - 1) E.cy++;
            break;
    }

    // Clamp tracking limits against variable raw row sizing metrics
    row_sz = (E.cy < E.num_rows) ? E.row[E.cy].size : 0;
    if (E.cx > row_sz) E.cx = row_sz;
}

void editorProcessKeypress(void) {
    int c = editorReadKey();

    switch (c) {
        case '\r': // ENTER KEY
            {
                int current_idx = editorGetBufferIndex(E.cx, E.cy);
                bufferMoveGap(&E.buffer, current_idx);
                bufferInsert(&E.buffer, '\n');
                E.cy++;
                E.cx = 0;
                E.dirty++;
            }
            break;

        case '\x11': // CTRL-Q TO QUIT
            if (E.dirty) {
                editorSetStatusMessage("WARNING: File has unsaved modifications! Press Ctrl-Q 2 more times to override and force quit.");
                static int quit_confirmations = 2;
                if (--quit_confirmations == 0) {
                    write(STDOUT_FILENO, "\x1b[2J", 4);
                    write(STDOUT_FILENO, "\x1b[H", 3);
                    editorCleanup();
                    exit(0);
                }
                break;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            editorCleanup();
            exit(0);
            break;

        case '\x13': // CTRL-S TO SAVE
            editorSave();
            break;

        case '\x06': // CTRL-F TO SEARCH
            editorFind();
            break;

        case BACKSPACE:
        case DEL_KEY:
            {
                int current_idx = editorGetBufferIndex(E.cx, E.cy);
                if (c == DEL_KEY) {
                    // Turn Delete into Backspace relative to index translation shifts
                    current_idx++;
                    if (current_idx > bufferGetTotalSize(&E.buffer)) break;
                    E.cx++;
                }
                
                if (current_idx > 0) {
                    bufferMoveGap(&E.buffer, current_idx);
                    
                    // Track if we are deleting an active newline mapping
                    int target_char = E.buffer.data[E.buffer.gap_start - 1];
                    bufferDelete(&E.buffer);
                    
                    if (target_char == '\n') {
                        editorUpdateRows();
                        E.cy--;
                        E.cx = E.row[E.cy].size;
                    } else {
                        E.cx--;
                    }
                    E.dirty++;
                }
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case HOME_KEY:
        case END_KEY:
        case PAGE_UP:
        case PAGE_DOWN:
            editorMoveCursor(c);
            break;

        default:
            if (c != '\0' && c < 1000) {
                int current_idx = editorGetBufferIndex(E.cx, E.cy);
                bufferMoveGap(&E.buffer, current_idx);
                bufferInsert(&E.buffer, c);
                E.cx++;
                E.dirty++;
            }
            break;
    }
}

/*** Orchestration Layer Execution Pipeline ***/
int main(int argc, char *argv[]) {
    enableRawMode();
    editorInit();
    
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("CoreEdit: Ctrl-S = Save | Ctrl-F = Find | Ctrl-Q = Quit");

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
