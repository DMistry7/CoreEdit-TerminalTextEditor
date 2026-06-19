#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
    char *data;       // The total memory buffer block
    int capacity;     // Total allocated size of data array
    int gap_start;    // Index where the empty gap begins
    int gap_end;      // Index where the empty gap ends
} GapBuffer;

void bufferInit(GapBuffer *gb, int initial_capacity);
void bufferFree(GapBuffer *gb);
void bufferMoveGap(GapBuffer *gb, int target_position);
void bufferInsert(GapBuffer *gb, char c);
void bufferDelete(GapBuffer *gb);

#endif
