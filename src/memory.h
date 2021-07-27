#ifndef cawit_memory_h
#define cawit_memory_h

#include "common.h"
#include "object.h"

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, sizeof(type) * (count))

#define FREE(pointer) reallocate(pointer, 0);

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, newCapacity) \
    (type*)reallocate(pointer, sizeof(type) * (newCapacity))

#define FREE_ARRAY(pointer) \
    reallocate(pointer, 0)

void* reallocate(void* pointer, size_t newSize);
void freeObjects();

#endif