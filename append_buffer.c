#include "append_buffer.h"
#include <stdlib.h>
#include <string.h>

void abAppend(AppendBuffer *ab, const char *s, int len) {
    char *new_block = realloc(ab->b, ab->len + len);
    if (new_block == NULL) return;
    memcpy(&new_block[ab->len], s, len);
    ab->b = new_block;
    ab->len += len;
}

void abFree(AppendBuffer *ab) {
    free(ab->b);
}
