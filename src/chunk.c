#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(chunk->code);
    FREE_ARRAY(chunk->lines);
    freeValueArray(&chunk->constants);
    chunk->count = 0;
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        chunk->capacity = GROW_CAPACITY(chunk->capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, 
            chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    if (chunk->lineCapacity < line) {
        chunk->lineCapacity = GROW_CAPACITY(chunk->lineCapacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines,
            chunk->lineCapacity);
    }

    chunk->lines[chunk->lineCount]++;
    if (chunk->lineCount < line) {
        chunk->lineCount++;
    }
}

void writeConstant(Chunk* chunk, Value value, int line) {
    int index = addConstant(chunk, value);
    if (index < 256) {
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, index, line);
    } else {
        writeChunk(chunk, OP_LONG_CONSTANT, line);
        writeChunk(chunk, (uint8_t)((index >> 16) & 0xFF), line);
        writeChunk(chunk, (uint8_t)((index >> 8) & 0xFF), line);
        writeChunk(chunk, (uint8_t)(index & 0xFF), line);
    }
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

int getLine(Chunk* chunk, int instruction) {
    for (int line = 0; line < chunk->lineCount; line++) {
        instruction -= chunk->lines[line];
        if (instruction <= 0) return line + 1;
    }
}