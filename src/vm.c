#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "common.h"
#include "vm.h"
#include "object.h"
#include "memory.h"
#include "compiler.h"
#include "debug.h"

VM vm;

static Value scanNative(int argCount, Value* args) {
    char input[1024];
    if (!fgets(input, sizeof(input), stdin)) {
        printf("Hindi mabasa ang ibinigay na halaga.\n");
        return NULL_VAL;
    }

    // fgets read the '\n' at the end that serves as the +1 for '\0'.
    int length = strlen(input);

    int i = 0;
    while (input[i] != '\0' && isdigit(input[i]))
        i++;

    if (i != length && input[i] == '.')
        while (input[i] != '\0' && isdigit(input[i]))
            i++;

    input[length - 1] = '\0'; // Replace '\n' with '\0'.
    if (i == length - 1)
        return NUMBER_VAL(strtod(input, NULL));
    else 
        return OBJ_VAL(copyString(input, length));
}

static Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[linya %d] sa ",
                getLine(&function->chunk, instruction));
        if (function->name == NULL) {
            fprintf(stderr, "skrip\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char* name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM() {
    resetStack();
    vm.objects = NULL;

    initTable(&vm.globals);
    initTable(&vm.strings);

    defineNative("clock", clockNative);
    defineNative("basahin", scanNative);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjFunction* function, int argCount) {
    if (argCount != function->arity) {
        runtimeError("Inaasahan na makakita ng %d argumento ngunit nakakita ng %d.",
            function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Umaapaw ang salansan.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_FUNCTION:
                return call(AS_FUNCTION(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break; // Non-callable object type.
        }
    }
    runtimeError("Mga gawain at mga uri lamang ang maaaring tawagin.");
    return false;
}

static bool isFalsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value)); 
}

static ObjString* toString(Value value, char *buffer) {
    ObjString* string;

    switch (value.type) {
        case VAL_BOOL: {
            buffer = AS_BOOL(value) ? "tama" : "mali";
            return copyString(buffer, 4);
        }
        case VAL_NULL: {
            buffer = "null";
            return copyString(buffer, 4);
        }
        case VAL_NUMBER: {
            int length = VAL_BUFFER_SIZE;
            length = snprintf(buffer, length, "%g", AS_NUMBER(value));
            return copyString(buffer, length);
        }
        case VAL_OBJ: return AS_STRING(value);
        default: {
            runtimeError(
                "Ang nilalaman ay hindi magawang salita.");
            return NULL;
        }
    }
}

static bool concatenate() {
    char bBuffer[VAL_BUFFER_SIZE];
    char aBuffer[VAL_BUFFER_SIZE];
    ObjString* b = toString(pop(), bBuffer);
    ObjString* a = toString(pop(), aBuffer);

    if (b == NULL || a == NULL) return false;

    int length = a->length + b->length;
    ObjString* result = makeString(length);
    memcpy(result->chars, a->chars, a->length);
    memcpy(result->chars + a->length, b->chars, b->length);
    result->chars[length] = '\0';
    
    push(OBJ_VAL(result));
    return true;
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)

#define READ_CONSTANT() \
    (frame->function->chunk.constants.values[READ_BYTE()])

#define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Inaasahang parehong numero ang gamit."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
    }

    printf("\n");
    disassembleInstruction(&frame->function->chunk,
        (int)(frame->ip - frame->function->chunk.code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_LONG_CONSTANT:
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NULL: push(NULL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP: pop(); break;
            case OP_DUP: push(peek(0)); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Hindi kilala ang lagayan '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Hindi kilala ang lagayan '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    BINARY_OP(NUMBER_VAL, +); break;
                } else if(!concatenate()) {
                    runtimeError("Hindi makabuo ng salita gamit.");
                    return INTERPRET_RUNTIME_ERROR;
				}
                break;
            }
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
            case OP_MODULO: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Inaasahang parehong numero ang gamit.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int b = AS_NUMBER(pop());
                int a = AS_NUMBER(pop());
                push(NUMBER_VAL(a % b));
                break;
            }
            case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("Inaasahang numero ang gamit.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_RETURN: {
                Value result = pop();
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    call(function, 0);

    return run();
}
