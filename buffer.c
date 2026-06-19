#include "buffer.h"
#include <stdlib.h>
#include <string.h>

void bufferInit(GapBuffer *gb, int initial_capacity) {
    gb->capacity = initial_capacity;
    gb->data = malloc(gb->capacity);
    gb->gap_start = 0;
    gb->gap_end = gb->capacity - 1;
}

void bufferFree(GapBuffer *gb) {
    free(gb->data);
    gb->data = NULL;
}

// Resizes the buffer when the gap runs out of space
static void bufferResize(GapBuffer *gb, int new_capacity) {
    int old_capacity = gb->capacity;
    int right_size = old_capacity - 1 - gb->gap_end;
    
    // Temporarily allocate new size or use realloc smartly
    char *new_data = malloc(new_capacity);
    
    // Copy text before the gap
    memcpy(new_data, gb->data, gb->gap_start);
    
    // Copy text after the gap to the end of the new buffer layout
    int new_gap_end = new_capacity - 1 - right_size;
    memcpy(new_data + new_gap_end + 1, gb->data + gb->gap_end + 1, right_size);
    
    free(gb->data);
    gb->data = new_data;
    gb->gap_end = new_gap_end;
    gb->capacity = new_capacity;
}

void bufferMoveGap(GapBuffer *gb, int target_position) {
    if (target_position < 0) target_position = 0;
    
    // Calculate characters to the left of the current gap
    while (gb->gap_start > target_position) {
        gb->gap_start--;
        gb->gap_end--;
        gb->data[gb->gap_end + 1] = gb->data[gb->gap_start];
    }
    // Calculate characters to the right of the current gap
    while (gb->gap_start < target_position) {
        // Stop if we hit the actual text bounds
        int right_size = gb->capacity - 1 - gb->gap_end;
        if (right_size <= 0) break;

        gb->data[gb->gap_start] = gb->data[gb->gap_end + 1];
        gb->gap_start++;
        gb->gap_end++;
    }
}

void bufferInsert(GapBuffer *gb, char c) {
    // If gap is empty, grow the array buffer size
    if (gb->gap_start > gb->gap_end) {
        bufferResize(gb, gb->capacity * 2);
    }
    
    gb->data[gb->gap_start] = c;
    gb->gap_start++;
}

void bufferDelete(GapBuffer *gb) {
    // Acts as a Backspace relative to gap position
    if (gb->gap_start > 0) {
        gb->gap_start--;
    }
}
