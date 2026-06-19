#ifndef TERMINAL_H
#define TERMINAL_H

#include <termios.h>

enum EditorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

void enableRawMode(void);
void disableRawMode(void);
int editorReadKey(void);
int getWindowSize(int *rows, int *cols);

#endif
