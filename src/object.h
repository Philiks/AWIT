#ifndef awit_object_h
#define awit_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value)     (objType(AS_OBJ(value)))

#define IS_CLOSURE(value)   isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)  isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)    isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)    isObjType(value, OBJ_STRING)

#define AS_CLOSURE(value)   ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)  ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)    ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} ObjType;

struct Obj {
    // MSB (1 byte):     bool mark
    // middle (6 bytes): Obj* next
    // LSB (1 byte):     ObjType type
    uint64_t header;
};

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char chars[];
};

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);
ObjString* makeString(int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

static inline bool objMark(Obj* object) {
    return (bool)((object->header >> 56) & 0x01);
}

static inline Obj* objNext(Obj* object) {
    return (Obj*)((object->header >> 8) & 0x00ffffffffffff);
}

static inline ObjType objType(Obj* object) {
    return (ObjType)(object->header & 0x000000000000000f);
}

static inline void setMark(Obj* object, bool mark) {
    object->header = (object->header & 0x00ffffffffffffff) |
        ((uint64_t)mark << 56);
}

static inline void setObjNext(Obj* object, Obj* next) {
    object->header = (object->header & 0xff000000000000ff) |
        ((uint64_t)next << 8);
}

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && objType(AS_OBJ(value)) == type;
}

#endif
