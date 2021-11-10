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

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
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

static bool isSameArity(int argCount, int funcArity) {
    if (argCount != funcArity) {
        runtimeError("Inaasahan na makakita ng %d argumento ngunit nakakita ng %d.",
            funcArity, argCount);
        return false;
    }

    return true;
}

static bool willNotOverflow() {
    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Umaapaw ang salansan.");
        return false;
    }

    return true;
}

static Value hasFieldNative(int argCount, Value* args) {
    if (!(isSameArity(argCount, 2) && willNotOverflow()))
        return BOOL_VAL(false);
    if (!(IS_INSTANCE(args[0]) && IS_STRING(args[1])))
        return BOOL_VAL(false);
    
    ObjInstance* instance = AS_INSTANCE(args[0]);
    Value dummy;
    return BOOL_VAL(tableGet(&instance->fields, AS_STRING(args[1]), &dummy));
}

static Value scanNative(int argCount, Value* args) {
    if (!(isSameArity(argCount, 0) && willNotOverflow()))
        return NULL_VAL;

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
    if (!(isSameArity(argCount, 0) && willNotOverflow()))
        return NULL_VAL;

    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
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
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;
    vm.markValue = false;

    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = copyString("sim", 3);

    defineNative("oras", clockNative);
    defineNative("basahin", scanNative);
    defineNative("mayKatangian", hasFieldNative);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
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

static bool call(ObjClosure* closure, int argCount) {
    if (!(isSameArity(argCount, closure->function->arity) &&
          willNotOverflow()))
        return false;

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_ARRAY: {
                // argCount here serves as the index of the element.
                ObjArray* array = AS_ARRAY(callee);
                if (argCount >= array->elements.count) {
                    runtimeError("Ang koleksyon ay naglalaman ng %d elemento ngunit nakatanggap ng %d.",
                        array->elements.count, argCount);
                    return false;
                }

                if (argCount < 0)
                    push(array->elements.values[array->elements.count + argCount]);
                else
                    push(array->elements.values[argCount]);
                return true;
            }
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
                Value initializer;
                if (tableGet(&klass->methods, vm.initString, &initializer)) {
                    return call(AS_CLOSURE(initializer), argCount);
                } else if (argCount != 0) {
                    runtimeError("Inaasahan na hindi makakita ng argumento ngunit nakatanggap ng %d.",
                        argCount);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
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

static bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Hindi kilala ang katangian '%s'.", name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
    Value receiver = peek(argCount);

    if (!IS_INSTANCE(receiver)) {
        runtimeError("Tanging mga instansya lamang ang may mga instansyang gawain.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);

    Value value;
    if (tableGet(&instance->fields, name, &value)) {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }

    return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Hindi kilala ang katangian '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }
    
    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    while (vm.openUpvalues != NULL &&
           vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static void defineMethod(ObjString* name) {
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

static bool isFalsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value)); 
}

static ObjString* toString(Value value, char* buffer) {
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
            length = snprintf(buffer, length, "%.0f", AS_NUMBER(value));
            return copyString(buffer, length);
        }
        case VAL_OBJ: {
            if (!IS_STRING(value)) {
                runtimeError(
                    "Ang halaga ay hindi magawang salita.");
                return NULL;
            }

            return AS_STRING(value);
        }
    }
}

static bool concatenate() {
    char bBuffer[VAL_BUFFER_SIZE];
    char aBuffer[VAL_BUFFER_SIZE];
    ObjString* b = toString(peek(0), bBuffer);
    ObjString* a = toString(peek(1), aBuffer);

    if (b == NULL || a == NULL) return false;

    int length = a->length + b->length;
    ObjString* result = makeString(length);
    memcpy(result->chars, a->chars, a->length);
    memcpy(result->chars + a->length, b->chars, b->length);
    result->chars[length] = '\0';

    pop();
    pop();
    push(OBJ_VAL(result));
    return true;
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    register uint8_t* ip = frame->ip;

#define READ_BYTE() (*ip++)

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_SHORT() \
    (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            frame->ip = ip; \
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
    disassembleInstruction(&frame->closure->function->chunk,
        (int)(ip - frame->closure->function->chunk.code));
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
                    frame->ip = ip;
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
                    frame->ip = ip;
                    runtimeError("Hindi kilala ang lagayan '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_ELEMENT: {
                Value index = pop();
                Value array = pop();

                frame->ip = ip;
                if (!IS_ARRAY(array)) {
                    runtimeError("Tanging koleksyon lamang ang maaaring tawagin gamit ang '[]'.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!IS_NUMBER(index)) {
                    runtimeError("Inaasahan na makatanggap ng numero bilang indeks.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!callValue(array, (int)AS_NUMBER(index))) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                ip = frame->ip;

                break;
            }
            case OP_DEFINE_ARRAY: {
                uint8_t elementCount = READ_BYTE();
                ObjArray* array = newArray();

                int i = elementCount;
                // Use peek() since stack values are in reversed order. 
                // i.e. [ 1, 2, 3, 4] -> [ 4, 3, 2, 1] in stack.
                while (i > 0)
                    writeValueArray(&array->elements, peek(--i));

                vm.stackTop -= elementCount; // Remove elemets.

                push(OBJ_VAL(array));
                break;
            }
            case OP_DECLARE_ARRAY: {
                // The element count here does not rely on the number of compiled expressions.
                // The element count comes from the "value" of the compiled expression arr[n].
                Value elementCount = pop();

                if (!IS_NUMBER(elementCount)) {
                    frame->ip = ip;
                    runtimeError("Inaasahan na makatanggap ng numero para sa bilang ng mga elemento.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (AS_NUMBER(elementCount) < 0) {
                    frame->ip = ip;
                    runtimeError("Inaasahan na makatanggap ng numero na higit sa 0 para sa bilang ng mga elemento.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjArray* array = newArray();

                int i = AS_NUMBER(elementCount);
                // The array will be initialized with NULL.
                while (i-- > 0)
                    writeValueArray(&array->elements, NULL_VAL);

                push(OBJ_VAL(array));
                break;
            }
            case OP_MULTI_ARRAY: {
                // - 1 was the rightmost array that was already processed.
                uint8_t dimension = READ_BYTE() - 1;

                while (dimension-- > 0) {
                    ObjArray* array = AS_ARRAY(pop());
                    int enclosingArraySize = (int)AS_NUMBER(pop());

                    ObjArray* enclosing = newArray();
                    while (enclosingArraySize-- > 0) {
                        ObjArray* element = newArray();
                        copyValueArray(&array->elements, &element->elements);
                        writeValueArray(&enclosing->elements, OBJ_VAL(element));
                    }

                    push(OBJ_VAL(enclosing));
                }

                break;
            }
            case OP_SET_ELEMENT: {
                Value value = pop();
                Value index = pop();
                Value array = pop();

                if (!IS_ARRAY(array)) {
                    frame->ip = ip;
                    runtimeError("Tanging koleksyon lamang ang maaaring tawagin gamit ang '[]'.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!IS_NUMBER(index)) {
                    frame->ip = ip;
                    runtimeError("Inaasahan na makatanggap ng numero bilang indeks.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                AS_ARRAY(array)->elements.values[(int)AS_NUMBER(index)] = value;
                push(value); // Leave the value on the stack.
                break;
            } 
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))) {
                    frame->ip = ip;
                    runtimeError("Tanging mga instansya lamang ang may mga katangian.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(peek(0));
                ObjString* name = READ_STRING();

                Value value;
                if (tableGet(&instance->fields, name, &value)) {
                    pop(); // Instance.
                    push(value);
                    break;
                }

                if (!bindMethod(instance->klass, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))) {
                    frame->ip = ip;
                    runtimeError("Tanging mga instansya lamang ang may mga katangian.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance* instance = AS_INSTANCE(peek(1));
                tableSet(&instance->fields, READ_STRING(), peek(0));
                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pop());

                if (!bindMethod(superclass, name)) {
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
                    frame->ip = ip;
                    runtimeError("Hindi makabuo ng salita gamit.");
                    return INTERPRET_RUNTIME_ERROR;
				}
                break;
            }
            case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
            case OP_MODULO: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    frame->ip = ip;
                    runtimeError("Inaasahang parehong numero ang gamit.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int b = AS_NUMBER(pop());
                int a = AS_NUMBER(pop());
                push(NUMBER_VAL(a % b));
                break;
            }
            case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
            case OP_INT_DIVIDE: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    frame->ip = ip;
                    runtimeError("Inaasahang parehong numero ang gamit.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL((int)(a / b)));
                break;
            }
            case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    frame->ip = ip;
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
                ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                ip -= offset;
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                frame->ip = ip;
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                frame->ip = ip;
                if (!invoke(method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop());
                frame->ip = ip;
                if (!invokeFromClass(superclass, method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] =
                            captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            case OP_RETURN: {
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;
                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                ip = frame->ip;
                break;
            }
            case OP_CLASS:
                push(OBJ_VAL(newClass(READ_STRING())));
                break;
            case OP_INHERIT: {
                Value superclass = peek(1);
                if (!IS_CLASS(superclass)) {
                    frame->ip = ip;
                    runtimeError("Uri lamang ang maaaring magpamana.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass* subclass = AS_CLASS(peek(0));
                tableAddAll(&AS_CLASS(superclass)->methods,
                            &subclass->methods);
                pop(); // Subclass.
                break;
            }
            case OP_METHOD:
                defineMethod(READ_STRING());
                break;
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
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}
