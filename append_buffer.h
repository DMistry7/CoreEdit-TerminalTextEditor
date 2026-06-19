#ifndef APPEND_BUFFER_H
#define APPEND_BUFFER_H

typedef struct {
    char *b;
    int len;
} AppendBuffer;

#define ABUF_INIT {NULL, 0}

void abAppend(AppendBuffer *ab, const char *s, int len);
void abFree(AppendBuffer *ab);

#endif
