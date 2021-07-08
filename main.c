#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
    Chunk chunk;
    initChunk(&chunk);

    writeConstant(&chunk, 1.2, 1);
    writeConstant(&chunk, 3.4, 2);

    writeChunk(&chunk, OP_RETURN, 3);
    writeChunk(&chunk, OP_RETURN, 4);
    writeChunk(&chunk, OP_RETURN, 5);
    writeChunk(&chunk, OP_RETURN, 5);
    writeChunk(&chunk, OP_RETURN, 6);

    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);
    return 0;
}