#include "terminal.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>

static struct termios orig_termios;

void disableRawMode(void) {
    // Restore original terminal attributes on exit
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode(void) {
    // Get current attributes
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr failed");
        exit(1);
    }
    
    // Register cleanup function to run automatically when exit() is called
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    
    // Input flags: disable break, CR-to-NL, parity check, strip 8th bit, flow control (Ctrl-S/Ctrl-Q)
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // Output flags: disable post-processing (like automatic \r insertion after \n)
    raw.c_oflag &= ~(OPOST);
    // Control flags: set character size to 8 bits
    raw.c_cflag |= (CS8);
    // Local flags: disable echo, canonical mode (line buffering), extended functions, signals (Ctrl-C/Ctrl-Z)
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    // Control characters: Read timeouts (non-blocking read setup)
    raw.c_cc[VMIN] = 0;  // Return as soon as there is any input
    raw.c_cc[VTIME] = 1; // Timeout after 100ms if no input

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr failed");
        exit(1);
    }
}

int editorReadKey(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1) return '\0';
    }

    // If we detect an escape sequence, process arrow keys
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }
        return '\x1b';
    }
    return c;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}
