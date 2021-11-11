#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "memory.h"

void initValueArray(ValueArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void copyValueArray(ValueArray* from, ValueArray* to) {
    to->count = from->count;
    to->capacity = from->capacity;
    to->values = GROW_ARRAY(Value, to->values, 0, to->capacity);

    // Using writeValueArray in a loop not necessary since the 
    // count and capacity is already given. The only thing left
    // is to allocate the values in "to" using GROW_ARRAY.
    for (int i = 0; i < to->count; i++)
        to->values[i] = from->values[i];
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, 
            oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "tama" : "mali");
            break;
        case VAL_NULL: printf("null"); break;
        case VAL_NUMBER: printf("%f", AS_NUMBER(value)); break;
        case VAL_OBJ: printObject(value); break;
    }
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:      return true;
        case VAL_NUMBER:    return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:       return AS_OBJ(a) == AS_OBJ(b);
        default:            return false; // Unreachable.
    }
}
