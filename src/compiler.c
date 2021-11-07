#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#define MAX_CASES 256
#define MAX_BREAKS 256

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // o
    PREC_AND,           // at
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // ! - ++ --
    PREC_POST_INC,      // ++ --
    PREC_CALL,          // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;
    
    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool hasSuperclass;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;

int innermostLoopStart = -1;
int innermostLoopExits[MAX_BREAKS]; // Can also be used on Switch.
int innermostLoopExitCount = -1;
int innermostLoopDepth = 0;         // Can also be used on Switch.

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[linya %d] Mali", token->line);

    if (token->type == TOKEN_DULO) {
        fprintf(stderr, " sa dulo");
    } else if (token->type == TOKEN_PROBLEMA) {
        // Nothing.
    } else {
        fprintf(stderr, " sa '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_PROBLEMA) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    
    errorAtCurrent(message);
}

static TokenType peek() {
    return parser.current.type;
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;        
}

static void emitConstant(Value value) {
    writeConstant(currentChunk(), value, parser.previous.line);
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Masyadong maraming nilalaman upang puntahan.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start,
                                             parser.previous.length);
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = "ito";
        local->name.length = 3;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitVariable(uint8_t setGetOp, int index) {
    emitByte(setGetOp);

    if (index < UINT8_MAX) {
        emitByte((uint8_t)index);
    } else {
        emitByte((uint8_t)((index >> 16) & 0xFF));
        emitBytes((uint8_t)((index >> 8) & 0xFF),
                  (uint8_t)(index & 0xFF));
    }
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Masyadong marami ang nilalaman ng pahayag.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn() {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
    } else {
        emitByte(OP_NULL);
    }

    emitByte(OP_RETURN);
}

static int makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    return constant;
}

static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL
            ? function->name->chars : "<skrip>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
            current->locals[current->localCount - 1].depth >
                current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static int identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Hindi mabasa ang laman ng lagayan kasabay ng pagdeklara nito.");
            }
            return i;
        }
    }

    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index,
                      bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Masyadong maraming 'closure' na pagkakakilanlan sa gawain.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    // This assumes that the local is NOT a LONG_CONSTANT.
    // Else the compiler will break. But it was not implemented
    // since it is a rare case and adding more complexity in the 
    // later stage of the interpreter is not worth it.
    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Masyadong maraming lalagyan ng halaga sa kasalukuyang gawain.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static void declareVariable() {
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Mayroon ng nagngangalang ganitong lagayan sa kasalukuyang nasasakupan.");
        }
    }

    addLocal(*name);
}

static int parseVariable(const char* errorMessage) {
    consume(TOKEN_PAGKAKAKILANLAN, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth =
        current->scopeDepth;
}

static void defineVariable(int global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t elementList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_KANANG_BRACKET)) {
        do {
            expression();
            if (argCount == 255) {
                error("Hindi maaaring magkaroon ng higit sa 255 na mga halaga");
            }
            argCount++;
        } while (match(TOKEN_KUWIT));
    }
    consume(TOKEN_KANANG_BRACKET, 
        "Inaasahan na makakita ng ']' matapos ang mga halaga.");
    return argCount;
}

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_KANANG_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Hindi maaaring magkaroon ng higit sa 255 na mga argumento");
            }
            argCount++;
        } while (match(TOKEN_KUWIT));
    }
    consume(TOKEN_KANANG_PAREN, 
        "Inaasahan na makakita ng ')' matapos ang mga argumento.");
    return argCount;
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)rule->precedence + 1);

    switch (operatorType) {
        case TOKEN_HINDI_PAREHO:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_PAREHO:          emitByte(OP_EQUAL); break;
        case TOKEN_HIGIT:           emitByte(OP_GREATER); break;
        case TOKEN_HIGIT_PAREHO:    emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_BABA:            emitByte(OP_LESS); break;
        case TOKEN_BABA_PAREHO:     emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_DAGDAG:          emitByte(OP_ADD); break;
        case TOKEN_BAWAS:           emitByte(OP_SUBTRACT); break;
        case TOKEN_MODULO:          emitByte(OP_MODULO); break;
        case TOKEN_BITUIN:          emitByte(OP_MULTIPLY); break;
        case TOKEN_PAHILIS:         emitByte(OP_DIVIDE); break;
        default: return; // Unreachable.
    }
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void element(bool canAssign) {
    // We can't read parser.previous for the variable name
    // since there will be cases like name[indexOne][indexTwo]
    // so we leave everything on the stack.
    expression();
    consume(TOKEN_KANANG_BRACKET, "Inaasahan na makakita ng ']' matapos ang ekspresyon.");
    
    if (canAssign) {
        if (match(TOKEN_KATUMBAS)) {
            expression();
            emitByte(OP_SET_ELEMENT);
        } else {
            emitByte(OP_GET_ELEMENT);
        }
    } else {
        emitByte(OP_GET_ELEMENT);
    }
}

static void array(bool canAssign) {
    uint8_t elementCount = elementList();
    emitBytes(OP_DEFINE_ARRAY, elementCount);
}

static void dot(bool canAssign) {
    consume(TOKEN_PAGKAKAKILANLAN, 
            "Inaasahan ang pangalan ng katangian matapos ang '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_KATUMBAS)) {
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_KALIWANG_PAREN)) {
        uint8_t argCount = argumentList();
        emitBytes(OP_INVOKE, name);
        emitByte(argCount);
    } else {
        emitBytes(OP_GET_PROPERTY, name);
    }
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_MALI: emitByte(OP_FALSE); break;
        case TOKEN_NULL: emitByte(OP_NULL); break;
        case TOKEN_TAMA: emitByte(OP_TRUE); break;
        default: return; // Unreachable.
    }
}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_KANANG_PAREN, 
        "Inasahan na makakita ng ')' matapos ang ekspresyon");
}

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, 
                                    parser.previous.length - 2)));
}

static void decrement(bool canAssign) {
    emitByte(OP_SUBTRACT);
}

static void increment(bool canAssign) {
    emitByte(OP_ADD);
}

static void postfixIncDec(int varIndex, uint8_t setOp) {
    ParseFn incRule = getRule(parser.current.type)->infix;

    // Current look of stack after function call.
    //                             // <varUnchanged>
    emitByte(OP_DUP);              // <varUnchanged> <varUnchanged>
    emitConstant(NUMBER_VAL(1));   // <varUnchanged> <varUnchanged> 1
    incRule(false);                // <varUnchanged> <varUnchanged> 1 <++/-->
    emitVariable(setOp, varIndex); // <varUnchanged> <varChanged>
    emitByte(OP_POP);              // <varUnchanged>
    advance(); // Consume ++ or --.
}

static void prefixIncDec(TokenType operatorType) {
    ParseFn incRule = getRule(operatorType)->infix;

    // Current look of stack after function call.
    //                             // <varUnchanged>
    emitConstant(NUMBER_VAL(1));   // <varUnchanged> 1
    incRule(false);                // <varUnchanged> 1 <++/-->

    int arg = resolveLocal(current, &parser.previous);
    uint8_t setOp;
    if (arg != -1) {
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&parser.previous);
        setOp = OP_SET_GLOBAL;
    }
    
    emitVariable(setOp, arg);      // <varChanged>
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1 ) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign) {
        if (match(TOKEN_KATUMBAS)) {
            expression();
            emitVariable(setOp, arg);
        } else if (check(TOKEN_BAWAS_ISA) || check(TOKEN_DAGDAG_ISA)) {
            emitVariable(getOp, arg);
            postfixIncDec(arg, setOp);
        } else {
            emitVariable(getOp, arg);
        }
    } else {
        emitVariable(getOp, arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static Token syntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void super_(bool canAssign) {
    if (currentClass == NULL) {
        error("Hindi maaaring gamitin ang 'mula' sa labas ng uri.");
    } else if (!currentClass->hasSuperclass) {
        error("Hindi maaaring gamitin ang 'mula' sa uri na walang pinagmamanahan.");
    }
     
    consume(TOKEN_TULDOK, "Inaasahan na makakita ng '.' matapos ang 'mula'.");
    consume(TOKEN_PAGKAKAKILANLAN, "Inaasahan ang pangalan ng gawain sa pinagmulang uri.");
    uint8_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("ito"), false);
    if (match(TOKEN_KALIWANG_PAREN)) {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("mula"), false);
        emitBytes(OP_SUPER_INVOKE, name);
        emitByte(argCount);
    } else {
        namedVariable(syntheticToken("mula"), false);
        emitBytes(OP_GET_SUPER, name);
    }
}

static void this_(bool canAssign) {
    if (currentClass == NULL) {
        error("Hindi maaaring gamitin ang 'ito' sa labas ng uri.");
        return;
    }

    variable(false);
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_HINDI: emitByte(OP_NOT); break;
        case TOKEN_BAWAS: emitByte(OP_NEGATE); break;
        case TOKEN_BAWAS_ISA:
        case TOKEN_DAGDAG_ISA: {
            prefixIncDec(operatorType);
            break;
        }
        default: return; // Unreachable.
    }
}

ParseRule rules[] = {
    [TOKEN_KALIWANG_PAREN]   = {grouping,  call,      PREC_CALL},
    [TOKEN_KANANG_PAREN]     = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KALIWANG_BRACE]   = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KANANG_BRACE]     = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KALIWANG_BRACKET] = {array,     element,   PREC_CALL},
    [TOKEN_KANANG_BRACKET]   = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KUWIT]            = {NULL,      NULL,      PREC_NONE},
    [TOKEN_TULDOK]           = {NULL,      dot,       PREC_CALL},
    [TOKEN_TULDOK_KUWIT]     = {NULL,      NULL,      PREC_NONE},
    [TOKEN_TUTULDOK]         = {NULL,      NULL,      PREC_NONE},
    [TOKEN_PAHILIS]          = {NULL,      binary,    PREC_FACTOR},
    [TOKEN_BITUIN]           = {NULL,      binary,    PREC_FACTOR},
    [TOKEN_MODULO]           = {NULL,      binary,    PREC_FACTOR},
    [TOKEN_BAWAS]            = {unary,     binary,    PREC_TERM},
    [TOKEN_BAWAS_ISA]        = {unary,     decrement, PREC_POST_INC},
    [TOKEN_DAGDAG]           = {NULL,      binary,    PREC_TERM},
    [TOKEN_DAGDAG_ISA]       = {unary,     increment, PREC_POST_INC},
    [TOKEN_HINDI]            = {unary,     NULL,      PREC_NONE},
    [TOKEN_HINDI_PAREHO]     = {NULL,      binary,    PREC_EQUALITY},
    [TOKEN_KATUMBAS]         = {NULL,      NULL,      PREC_NONE},
    [TOKEN_PAREHO]           = {NULL,      binary,    PREC_EQUALITY},
    [TOKEN_HIGIT]            = {NULL,      binary,    PREC_COMPARISON},
    [TOKEN_HIGIT_PAREHO]     = {NULL,      binary,    PREC_COMPARISON},
    [TOKEN_BABA]             = {NULL,      binary,    PREC_COMPARISON},
    [TOKEN_BABA_PAREHO]      = {NULL,      binary,    PREC_COMPARISON},
    [TOKEN_PAGKAKAKILANLAN]  = {variable,  NULL,      PREC_NONE},
    [TOKEN_SALITA]           = {string,    NULL,      PREC_NONE},
    [TOKEN_NUMERO]           = {number,    NULL,      PREC_NONE},
    [TOKEN_AT]               = {NULL,      and_,      PREC_AND},
    [TOKEN_GAWAIN]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_GAWIN]            = {NULL,      NULL,      PREC_NONE},
    [TOKEN_HABANG]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_IBALIK]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_IPAKITA]          = {NULL,      NULL,      PREC_NONE},
    [TOKEN_ITIGIL]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_ITO]              = {this_,     NULL,      PREC_NONE},
    [TOKEN_ITULOY]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KADA]             = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KAPAG]            = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KILALANIN]        = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KUNDIMAN]         = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KUNG]             = {NULL,      NULL,      PREC_NONE},
    [TOKEN_MALI]             = {literal,   NULL,      PREC_NONE},
    [TOKEN_MULA]             = {super_,    NULL,      PREC_NONE},
    [TOKEN_NULL]             = {literal,   NULL,      PREC_NONE},
    [TOKEN_O]                = {NULL,      or_,       PREC_OR},
    [TOKEN_PALYA]            = {NULL,      NULL,      PREC_NONE},
    [TOKEN_SURIIN]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_TAMA]             = {literal,   NULL,      PREC_NONE},
    [TOKEN_URI]              = {NULL,      NULL,      PREC_NONE},
    [TOKEN_PROBLEMA]         = {NULL,      NULL,      PREC_NONE},
    [TOKEN_DULO]             = {NULL,      NULL,      PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Inasahan na makakita ng ekspresyon.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_KATUMBAS)) {
        error("Mali ang itinuturong lalagyan ng halaga.");
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_KANANG_BRACE) && !check(TOKEN_DULO)) {
        declaration();
    }

    consume(TOKEN_KANANG_BRACE, 
        "Inaasahan na makakita ng '}' matapos ang mga pahayag.");
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_KALIWANG_PAREN, 
        "Inaasahan na makakita ng '(' matapos ang pangalan ng gawain.");
    if (!check(TOKEN_KANANG_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Hindi maaaring magkaroon ng mahigit sa 255 na parametro.");
            }
            uint8_t constant = parseVariable(
                "Inaasahan na makakita ng pangalan ng parametro.");
            defineVariable(constant);
        } while (match(TOKEN_KUWIT));
    }
    consume(TOKEN_KANANG_PAREN, 
        "Inaasahan na makakita ng ')' matapos ang mga parametro.");
    consume(TOKEN_KALIWANG_BRACE, 
        "Inaasahan na makakita ng '{' bago ang mga pahayag sa gawain.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method() {
    consume(TOKEN_PAGKAKAKILANLAN, "Inaasahan ang pangalan ng instansyang gawain.");
    uint8_t constant = identifierConstant(&parser.previous);

    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 3 &&
        memcmp(parser.previous.start, "sim", 3) == 0) {
        type = TYPE_INITIALIZER;
    }

    function(type);
    emitBytes(OP_METHOD, constant);
}

static void classDeclaration() {
    consume(TOKEN_PAGKAKAKILANLAN, "Inaasahan ang pangalan ng uri.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (match(TOKEN_BABA)) {
        consume(TOKEN_PAGKAKAKILANLAN, 
            "Inaasahan ang pangalan ng pagmamanahang uri.");
        variable(false);

        if (identifiersEqual(&className, &parser.previous)) {
            error("Hindi maaaring magmana ang uri sa kanyang sarili.");
        }

        beginScope();
        addLocal(syntheticToken("mula"));
        defineVariable(0);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);
    consume(TOKEN_KALIWANG_BRACE, 
        "Inaasahan na makakita ng '{' bago ang mga pahayag sa uri.");
    while (!check(TOKEN_KANANG_BRACE) && !check(TOKEN_DULO)) {
        method();
    }
    consume(TOKEN_KANANG_BRACE, 
        "Inaasahan na makakita ng '}' matapos ang mga pahayag sa uri.");
    emitByte(OP_POP);

    if (classCompiler.hasSuperclass) {
        endScope();
    }

    currentClass = currentClass->enclosing;
}

static void funDeclaration() {
    uint8_t global = parseVariable("Inaasahan ang pangalan ng gawain.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void declareArray() {
    int dimension = 0;

    do {
        dimension++;

        consume(TOKEN_KALIWANG_BRACKET,
                "Inaasahan na makakita ng '[' bago ang indeks.");
       
        // Array Size.
        if (check(TOKEN_KANANG_BRACKET)) emitConstant(NUMBER_VAL(0));
        else expression();

        // The stack will continuously store array sizes if it is a multi dimensional 
        // array and emit an OP_MULTI_ARRAY.
        // Otherwise it will only have one array size and emit an OP_DECLARE_ARRAY.

        consume(TOKEN_KANANG_BRACKET,
                "Inaasahan na makakita ng ']' matapos ang indeks.");
       
        if (check(TOKEN_KATUMBAS))
            error("Hindi maaaring maglagay ng paunang laman sa isang koleksyon matapos itong ideklara.");
    } while (!check(TOKEN_TULDOK_KUWIT) && !check(TOKEN_DULO));

    if (dimension == 1) {
        emitByte(OP_DECLARE_ARRAY);
        return;
    }

    if (dimension > UINT8_MAX) {
        error("Masyadong maraming koleksyon para sa isang pagkakakilanlan.");
    }

    // MULTI_ARRAY needs the rightmost array on top of the stack since its values
    // will be copied into the enclosing array.
    emitByte(OP_DECLARE_ARRAY);
    emitBytes(OP_MULTI_ARRAY, (uint8_t)dimension);
}

static void varDeclaration() {
    int global = parseVariable("Inasahan ang pangalan ng lalagyan ng nilalaman.");

    if (match(TOKEN_KATUMBAS)) {
        expression();
    } else if (check(TOKEN_KALIWANG_BRACKET)) {
        declareArray();
    } else {
        emitByte(OP_NULL);
    }
    consume(TOKEN_TULDOK_KUWIT, 
            "Inasahan na makakita ng ';' matapos ideklara ang lalagyan.");

    defineVariable(global);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_TULDOK_KUWIT, 
        "Inasahan na makakita ng ';' pagtapos ng ekspresyon.");
    emitByte(OP_POP);
}

static void forStatement() {
    beginScope();

    int loopVariable = -1;
    Token loopVariableName;
    loopVariableName.start = NULL;

    consume(TOKEN_KALIWANG_PAREN, 
        "Inasahan na makakita ng '(' matapos ang 'kada'.");
    if (match(TOKEN_TULDOK_KUWIT)) {
        // No initializer.
    } else if (match(TOKEN_KILALANIN)) {
        loopVariableName = parser.current;
        varDeclaration();
        loopVariable = current->localCount - 1;
    } else {
        expressionStatement();
    }

    int surroundingLoopExitCount = innermostLoopExitCount == -1 ?
                                     0 : innermostLoopExitCount;
    innermostLoopExitCount = 0;

    int surroundingLoopStart = innermostLoopStart;
    int surroundingLoopScopeDepth = innermostLoopDepth;
    innermostLoopStart = currentChunk()->count;
    innermostLoopDepth = current->scopeDepth;

    int exitJump = -1;
    if (!match(TOKEN_TULDOK_KUWIT)) {
        expression();
        consume(TOKEN_TULDOK_KUWIT, 
            "Inasahan na makakita ng ';' matapos ang kondisyon.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.                    
    }

    if (!match(TOKEN_KANANG_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_KANANG_PAREN, 
            "Inasahan na makakita ng ')' matapos ang mga payahag sa 'kada'.");

        emitLoop(innermostLoopStart);
        innermostLoopStart = incrementStart;
        patchJump(bodyJump);
    }

    int innerVariable = -1;
    if (loopVariable != -1) {
        beginScope();
        emitBytes(OP_GET_LOCAL, (uint8_t)loopVariable);
        addLocal(loopVariableName);
        markInitialized();
        innerVariable = current->localCount - 1;
    }

    statement();
    if (loopVariable != -1) {
        emitBytes(OP_GET_LOCAL, (uint8_t)innerVariable);
        emitBytes(OP_SET_LOCAL, (uint8_t)loopVariable);
        emitByte(OP_POP);

        endScope();
    }

    emitLoop(innermostLoopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition.
    }

    for (int i = surroundingLoopExitCount; 
         i < innermostLoopExitCount + surroundingLoopExitCount; 
         i++) {
        patchJump(innermostLoopExits[i]);
    }

    // ExitCount also acts as state so 0 count should be -1.
    innermostLoopExitCount = surroundingLoopExitCount == 0 ?
                               -1 : surroundingLoopExitCount;
    innermostLoopStart = surroundingLoopStart;
    innermostLoopDepth = surroundingLoopScopeDepth;

    endScope();
}

static void ifStatement() {
    consume(TOKEN_KALIWANG_PAREN, 
        "Inasahan na makakita ng '(' matapos ang 'kung'.");
    expression();
    consume(TOKEN_KANANG_PAREN, 
        "Inasahan na makakita ng ')' matapos ang kundisyon.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_KUNDIMAN)) statement();

    patchJump(elseJump);
}

static void switchStatement() {
    consume(TOKEN_KALIWANG_PAREN, 
        "Inasahan na makakita ng '(' matapos ang 'suriin'.");
    expression();
    current->localCount++; // Offset +1 to the stack pointer.
    consume(TOKEN_KANANG_PAREN, 
        "Inasahan na makakita ng ')' matapos ang ekspresyon.");

    consume(TOKEN_KALIWANG_BRACE, 
        "Inaasahan na makakita ng '{' bago ang 'kapag'.");

    int surroundingLoopExitCount = innermostLoopExitCount == -1 ?
                                     0 : innermostLoopExitCount;
    innermostLoopExitCount = 0;

    int state = 0; // 0: before all cases (kapag), 1: before default (palya), 2: after default (palya).
    int caseEnds[MAX_CASES];
    int caseCount = 0;
    int previousCaseSkip = -1;
    
    while (!match(TOKEN_KANANG_BRACE) && !check(TOKEN_DULO)) {
        if (match(TOKEN_KAPAG) || match(TOKEN_PALYA)) {
            TokenType caseType = parser.previous.type;

            if (state == 2) {
                error("Hindi na maaaring magdagdag pa ng 'kapag' o isa pang 'palya' matapos ang naunang 'palya'.");
            }

            if (state == 1) {
                // At the end of the previous case:
                // - end its scope.
                endScope();
                // - jump over the others.
                caseEnds[caseCount++] = emitJump(OP_JUMP);

                // Patch its condition to jump to the next case (this one).
                patchJump(previousCaseSkip);
                emitByte(OP_POP);
            }

            if (caseType == TOKEN_KAPAG) {
                beginScope();
                state = 1;

                // See if the case is equal to the value.
                emitByte(OP_DUP);
                expression();

                consume(TOKEN_TUTULDOK, "Inaasahan na makakita ng ':' matapos ang halaga sa 'kapag'.");

                emitByte(OP_EQUAL);
                previousCaseSkip = emitJump(OP_JUMP_IF_FALSE);

                // Pop the comparison result.
                emitByte(OP_POP);
            } else {
                // Check if there are no cases (default-only switch).
                if (state == 0) {
                    error("Inaasahan na makakita ng kahit isang 'kapag' bago ang 'palya'.");
                }

                state = 2;
                consume(TOKEN_TUTULDOK, "Inaasahan na makakita ng ':' matapos ang halaga sa 'palya'.");
                previousCaseSkip = -1;
            }
        } else {
            // Otherwise, it's a statement inside the current case.
            if (state == 0) {
                error("Hindi maaari ang mga pahayag bago ang 'kapag'.");
            }
            statement();
        }
    }

    for (int i = surroundingLoopExitCount; 
         i < innermostLoopExitCount + surroundingLoopExitCount; 
         i++) {
        patchJump(innermostLoopExits[i]);
    }

    // If we ended without a default case, patch its condition jump.
    if (state == 1) {
        endScope();
        caseEnds[caseCount++] = emitJump(OP_JUMP);

        patchJump(previousCaseSkip);
        emitByte(OP_POP);
    }

    // If we ended without any case, report an error. 
    // default-only situation was handled inside the loop.
    if (state == 2 && caseCount == 0) {
        error("Inaasahan na makakita ng kahit isang 'kapag' sa loob ng 'suriin' na pahayag.");
    }

    // Patch all the case jumps to the end.
    for (int i = 0; i < caseCount; i++) {
        patchJump(caseEnds[i]);
    }

    // ExitCount also acts as state so 0 count should be -1.
    innermostLoopExitCount = surroundingLoopExitCount == 0 ?
                               -1 : surroundingLoopExitCount;

    emitByte(OP_POP); // The switch value.
}

static void printStatement() {
    expression();
    consume(TOKEN_TULDOK_KUWIT, 
        "Inasahan na makakita ng ';' matapos ang nilalaman.");
    emitByte(OP_PRINT);
}

static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Hindi maaaring bumalik mula sa pinaka tuktok na code.");
    }

    if (match(TOKEN_TUTULDOK)) {
        emitReturn();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Hindi maaaring magbalik ng halaga sa loob ng tagapag simula.");
        }

        expression();
        consume(TOKEN_TULDOK_KUWIT,
            "Inasahan na makakita ng ';' matapos ang ibabalik na halaga.");
        emitByte(OP_RETURN);
    }
}

static void discardInnerLocals() {
    // Discard any locals created inside the loop.
    for (int i = current->localCount -1;
         i >= 0 && current->locals[i].depth > innermostLoopDepth;
         i--) {
        emitByte(OP_POP);
    }
}

static void continueStatement() {
    if (innermostLoopStart  == -1) {
        error("Hindi maaaring gamitin ang 'ituloy' sa labas ng loop.");
    }

    consume(TOKEN_TULDOK_KUWIT, 
        "Inasahan na makakita ng ';' matapos ang nilalaman.");

    discardInnerLocals();

    // Jump to top of current innermost loop.
    emitLoop(innermostLoopStart);
}

static void breakStatement() {
    if (innermostLoopExitCount == -1) {
        error("Hindi maaaring gamitin ang 'itigil' sa labas ng loop o labas ng 'suriin' na pahayag.");
    }

    consume(TOKEN_TULDOK_KUWIT, 
        "Inasahan na makakita ng ';' matapos ang nilalaman.");
         
    discardInnerLocals();

    // Jump unconditionally outside the loop or switch.
    // To be patched once all statements have been compiled.
    innermostLoopExits[innermostLoopExitCount++] = emitJump(OP_JUMP);
}

static void whileStatement() {
    int surroundingLoopExitCount = innermostLoopExitCount == -1 ?
                                     0 : innermostLoopExitCount;
    innermostLoopExitCount = 0;

    int surroundingLoopStart = innermostLoopStart;
    int surroundingLoopScopeDepth = innermostLoopDepth;
    innermostLoopStart = currentChunk()->count;
    innermostLoopDepth = current->scopeDepth;

    consume(TOKEN_KALIWANG_PAREN, 
        "Inasahan na makakita ng '(' matapos ang 'habang'.");
    expression();
    consume(TOKEN_KANANG_PAREN, 
        "Inasahan na makakita ng ')' matapos ang kondisyon.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(innermostLoopStart);

    patchJump(exitJump);
    emitByte(OP_POP);

    for (int i = surroundingLoopExitCount;
         i < innermostLoopExitCount + surroundingLoopExitCount; 
         i++) {
        patchJump(innermostLoopExits[i]);
    }

    // ExitCount also acts as state so 0 count should be -1.
    innermostLoopExitCount = surroundingLoopExitCount == 0 ?
                               -1 : surroundingLoopExitCount;
    innermostLoopStart = surroundingLoopStart;
    innermostLoopDepth = surroundingLoopScopeDepth;
}

static void doWhileStatement() {
    int surroundingLoopExitCount = innermostLoopExitCount == -1 ?
                                     0 : innermostLoopExitCount;
    innermostLoopExitCount = 0;

    int surroundingLoopStart = innermostLoopStart;
    int surroundingLoopScopeDepth = innermostLoopDepth;
    innermostLoopStart = currentChunk()->count;
    innermostLoopDepth = current->scopeDepth;

    statement();

    consume(TOKEN_HABANG, 
        "Inaasahan na makakita ng 'habang' matapos ang mga pahayag.");
    consume(TOKEN_KALIWANG_PAREN, 
        "Inasahan na makakita ng '(' matapos ang 'habang'.");
    expression();
    consume(TOKEN_KANANG_PAREN, 
        "Inasahan na makakita ng ')' matapos ang kondisyon.");
    consume(TOKEN_TULDOK_KUWIT, 
        "Inasahan na makakita ng ';' matapos ang nilalaman.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    emitLoop(innermostLoopStart);

    patchJump(exitJump);
    emitByte(OP_POP);

    for (int i = surroundingLoopExitCount;
         i < innermostLoopExitCount + surroundingLoopExitCount; 
         i++) {
        patchJump(innermostLoopExits[i]);
    }

    // ExitCount also acts as state so 0 count should be -1.
    innermostLoopExitCount = surroundingLoopExitCount == 0 ?
                               -1 : surroundingLoopExitCount;
    innermostLoopStart = surroundingLoopStart;
    innermostLoopDepth = surroundingLoopScopeDepth;
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_DULO) {
        if (parser.previous.type == TOKEN_TULDOK_KUWIT) return;
        switch (parser.current.type) {
            case TOKEN_URI:
            case TOKEN_GAWAIN:
            case TOKEN_KILALANIN:
            case TOKEN_KADA:
            case TOKEN_KUNG:
            case TOKEN_GAWIN:
            case TOKEN_HABANG:
            case TOKEN_IPAKITA:
            case TOKEN_IBALIK:
                return;

            default:
                ; // Do nothing.
        }

        advance();
    }
}

static void declaration() {
    if (match(TOKEN_URI)) {
        classDeclaration();
    } else if (match(TOKEN_GAWAIN)) {
        funDeclaration();
    } else if (match(TOKEN_KILALANIN)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_IPAKITA)) {
        printStatement();
    } else if (match(TOKEN_KADA)) {
        forStatement();
    } else if(match(TOKEN_KUNG)) {
        ifStatement();
    } else if(match(TOKEN_SURIIN)) {
        switchStatement();
    } else if(match(TOKEN_GAWIN)) {
        doWhileStatement();
    } else if (match(TOKEN_HABANG)) {
        whileStatement();
    } else if (match(TOKEN_IBALIK)) {
        returnStatement();
    } else if (match(TOKEN_ITULOY)) {
        continueStatement();
    } else if (match(TOKEN_ITIGIL)) {
        breakStatement();
    } else if (match(TOKEN_KALIWANG_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

ObjFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    
    while (!match(TOKEN_DULO)) {
        declaration();
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
