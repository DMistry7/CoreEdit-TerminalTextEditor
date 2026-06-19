#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
    char *data;
    int capacity;
    int gap_start;
    int gap_end;
} GapBuffer;

typedef struct {
    int size;
    int chars_index; // Global structural index offset within the raw Gap Buffer
    char *chars;     // Transient direct render slice
} EditorRow;

void bufferInit(GapBuffer *gb, int initial_capacity);
void bufferFree(GapBuffer *gb);
void bufferMoveGap(GapBuffer *gb, int target_position);
void bufferInsert(GapBuffer *gb, char c);
void bufferDelete(GapBuffer *gb);
int bufferGetTotalSize(GapBuffer *gb);
char *bufferToString(GapBuffer *gb, int *out_len);

#endif
