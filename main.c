#include "terminal.h"
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    GapBuffer buffer;
    int cursor_pos; // Logical text index position
    int screen_rows;
    int screen_cols;
} EditorState;

EditorState E;

void editorInit(void) {
    bufferInit(&E.buffer, 64);
    E.cursor_pos = 0;
    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        perror("Failed to detect terminal window size");
        exit(1);
    }
}

void editorRefreshScreen(void) {
    // Build a tiny render string via ANSI codes
    // \x1b[2J clears screen, \x1b[H homes cursor back to 1,1
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    // Print lines from the gap buffer safely skipping the empty gap zone
    for (int i = 0; i < E.buffer.gap_start; i++) {
        write(STDOUT_FILENO, &E.buffer.data[i], 1);
    }
    for (int i = E.buffer.gap_end + 1; i < E.buffer.capacity; i++) {
        write(STDOUT_FILENO, &E.buffer.data[i], 1);
    }
}

void editorProcessKeypress(void) {
    int c = editorReadKey();

    switch (c) {
        case '\x11': // Ctrl-Q to Quit program
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            bufferFree(&E.buffer);
            exit(0);
            break;

        case ARROW_LEFT:
            if (E.cursor_pos > 0) {
                E.cursor_pos--;
                bufferMoveGap(&E.buffer, E.cursor_pos);
            }
            break;

        case ARROW_RIGHT:
            // Calculate total non-gap chars currently stored
            int total_chars = E.buffer.gap_start + (E.buffer.capacity - 1 - E.buffer.gap_end);
            if (E.cursor_pos < total_chars) {
                E.cursor_pos++;
                bufferMoveGap(&E.buffer, E.cursor_pos);
            }
            break;

        case '\x7f': // Backspace character
            bufferDelete(&E.buffer);
            if (E.cursor_pos > 0) E.cursor_pos--;
            break;

        default:
            // Insert standard typed character
            if (c != '\0' && c < 1000) {
                bufferInsert(&E.buffer, c);
                E.cursor_pos++;
            }
            break;
    }
}

int main(void) {
    enableRawMode();
    editorInit();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
