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

static void bufferResize(GapBuffer *gb, int new_capacity) {
    int old_capacity = gb->capacity;
    int right_size = old_capacity - 1 - gb->gap_end;
    char *new_data = malloc(new_capacity);
    
    memcpy(new_data, gb->data, gb->gap_start);
    int new_gap_end = new_capacity - 1 - right_size;
    memcpy(new_data + new_gap_end + 1, gb->data + gb->gap_end + 1, right_size);
    
    free(gb->data);
    gb->data = new_data;
    gb->gap_end = new_gap_end;
    gb->capacity = new_capacity;
}

int bufferGetTotalSize(GapBuffer *gb) {
    return gb->gap_start + (gb->capacity - 1 - gb->gap_end);
}

void bufferMoveGap(GapBuffer *gb, int target_position) {
    if (target_position < 0) target_position = 0;
    int total = bufferGetTotalSize(gb);
    if (target_position > total) target_position = total;

    while (gb->gap_start > target_position) {
        gb->gap_start--;
        gb->gap_end--;
        gb->data[gb->gap_end + 1] = gb->data[gb->gap_start];
    }
    while (gb->gap_start < target_position) {
        gb->data[gb->gap_start] = gb->data[gb->gap_end + 1];
        gb->gap_start++;
        gb->gap_end++;
    }
}

void bufferInsert(GapBuffer *gb, char c) {
    if (gb->gap_start > gb->gap_end) {
        bufferResize(gb, gb->capacity * 2);
    }
    gb->data[gb->gap_start] = c;
    gb->gap_start++;
}

void bufferDelete(GapBuffer *gb) {
    if (gb->gap_start > 0) {
        gb->gap_start--;
    }
}

char *bufferToString(GapBuffer *gb, int *out_len) {
    int total_size = bufferGetTotalSize(gb);
    *out_len = total_size;
    char *str = malloc(total_size + 1);
    if (!str) return NULL;
    
    memcpy(str, gb->data, gb->gap_start);
    int right_size = gb->capacity - 1 - gb->gap_end;
    memcpy(str + gb->gap_start, gb->data + gb->gap_end + 1, right_size);
    str[total_size] = '\0';
    return str;
}
