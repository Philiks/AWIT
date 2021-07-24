#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk* compilingChunk;

static Chunk* currentChunk() {
    return compilingChunk;
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

static void emitConstant(Value value) {
    writeConstant(currentChunk(), value, parser.previous.line);
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void binary() {
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
        case TOKEN_BITUIN:          emitByte(OP_MULTIPLY); break;
        case TOKEN_PAHILIS:         emitByte(OP_DIVIDE); break;
        default: return; // Unreachable
    }
}

static void literal() {
    switch (parser.previous.type) {
        case TOKEN_MALI: emitByte(OP_FALSE); break;
        case TOKEN_NULL: emitByte(OP_NULL); break;
        case TOKEN_TAMA: emitByte(OP_TRUE); break;
        default: return; // Unreachable.
    }
}

static void grouping() {
    expression();
    consume(TOKEN_KANANG_PAREN, 
        "Inasahan na makakita ng ')' matapos ang ekspresyon");
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string() {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void decrement() {
    emitConstant(NUMBER_VAL(1));
    emitByte(OP_SUBTRACT);
}

static void increment() {
    emitConstant(NUMBER_VAL(1));
    emitByte(OP_ADD);
}

static void unary() {
    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_HINDI: emitByte(OP_NOT); break;
        case TOKEN_BAWAS: emitByte(OP_NEGATE); break;
        case TOKEN_BAWAS_ISA: {
            decrement();
            break;
        }
        case TOKEN_DAGDAG_ISA: {
            increment();
            break;
        }
        default: return; // Unreachable.
    }
}

ParseRule rules[] = {
    [TOKEN_KALIWANG_PAREN]   = {grouping,  NULL,      PREC_NONE},
    [TOKEN_KANANG_PAREN]     = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KUWIT]            = {NULL,      NULL,      PREC_NONE},
    [TOKEN_TULDOK]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_TULDOK_KUWIT]     = {NULL,      NULL,      PREC_NONE},
    [TOKEN_PAHILIS]          = {NULL,      binary,    PREC_FACTOR},
    [TOKEN_BITUIN]           = {NULL,      binary,    PREC_FACTOR},
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
    [TOKEN_PAGKAKAKILANLAN]  = {NULL,      NULL,      PREC_NONE},
    [TOKEN_SALITA]           = {string,    NULL,      PREC_NONE},
    [TOKEN_NUMERO]           = {number,    NULL,      PREC_NONE},
    [TOKEN_AT]               = {NULL,      NULL,      PREC_NONE},
    [TOKEN_GAWAIN]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_GAWIN]            = {NULL,      NULL,      PREC_NONE},
    [TOKEN_HABANG]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_IBALIK]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_IPAKITA]          = {NULL,      NULL,      PREC_NONE},
    [TOKEN_ITIGIL]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_ITO]              = {NULL,      NULL,      PREC_NONE},
    [TOKEN_ITULOY]           = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KADA]             = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KILALANIN]        = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KUNDIMAN]         = {NULL,      NULL,      PREC_NONE},
    [TOKEN_KUNG]             = {NULL,      NULL,      PREC_NONE},
    [TOKEN_MALI]             = {literal,   NULL,      PREC_NONE},
    [TOKEN_MULA]             = {NULL,      NULL,      PREC_NONE},
    [TOKEN_NGUNIT_KUNG]      = {NULL,      NULL,      PREC_NONE},
    [TOKEN_NULL]             = {literal,   NULL,      PREC_NONE},
    [TOKEN_O]                = {NULL,      NULL,      PREC_NONE},
    [TOKEN_TAMA]             = {literal,   NULL,      PREC_NONE},
    [TOKEN_URI]              = {NULL,      NULL,      PREC_NONE},
    [TOKEN_URONG]            = {NULL,      NULL,      PREC_NONE},
    [TOKEN_DULONG_URONG]     = {NULL,      NULL,      PREC_NONE},
    [TOKEN_PATLANG]          = {NULL,      NULL,      PREC_NONE},
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

    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

bool compile(const char* source, Chunk* chunk) {
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_DULO, "Inasahan na dulo na ng ekspresyon.");
    endCompiler();
    return !parser.hadError;
}