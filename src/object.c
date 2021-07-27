#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString* makeString(int length) {
    ObjString* string = (ObjString*)allocateObject(
        sizeof(ObjString) + length + 1, OBJ_STRING);
    string->length = length;
    return string;
}

static void hashStringObjSet(ObjString* string, uint32_t hash) {
    string->hash = hash;
    tableSet(&vm.strings, string, NULL_VAL);
}

ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm.strings, chars, length,
                                            hash);
    if (interned != NULL) return interned;

    ObjString* string = makeString(length);

    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';

    hashStringObjSet(string, hash);
    return string;
}

void hashStringObj(ObjString* string) {
    hashStringObjSet(string, hashString(string->chars, string->length));
}

void internedString(ObjString* string) {
    ObjString* interned = tableFindString(&vm.strings, string->chars, 
                                        string->length, string->hash);
    if (interned != NULL) {
        FREE(string);
        string = interned;
    }
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            return;
    }
}