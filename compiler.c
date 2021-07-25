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

typedef void (*ParseFn)(bool canAssign);

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

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
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
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start,
                                            name->length)));
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
        case TOKEN_BITUIN:          emitByte(OP_MULTIPLY); break;
        case TOKEN_PAHILIS:         emitByte(OP_DIVIDE); break;
        default: return; // Unreachable.
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

static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void decrement(bool canAssign) {
    emitByte(OP_SUBTRACT);
}

static void increment(bool canAssign) {
    emitByte(OP_ADD);
}

static void incrementRule(TokenType operatorType, bool canAssign, bool isPostfix) {
    ParseFn incRule = getRule(operatorType)->infix;

    // Put "1" on to the stack.
    emitConstant(NUMBER_VAL(1));

    // Apply increment() or decrement().
    incRule(canAssign);

    // Set the value to global variable.
    uint8_t arg = identifierConstant(&parser.previous);
    emitBytes(OP_SET_GLOBAL, arg);

    if (isPostfix) advance(); // Consume the Token ++ or --.
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t arg = identifierConstant(&name);

    if (canAssign) {
        if (match(TOKEN_KATUMBAS)) {
            expression();
            emitBytes(OP_SET_GLOBAL, arg);
        } else if (check(TOKEN_BAWAS_ISA)) {
            emitBytes(OP_GET_GLOBAL, arg);
            incrementRule(TOKEN_BAWAS_ISA, canAssign, true);
        } else if (check(TOKEN_DAGDAG_ISA)) {
            emitBytes(OP_GET_GLOBAL, arg);
            incrementRule(TOKEN_DAGDAG_ISA, canAssign, true);
        } else {
            emitBytes(OP_GET_GLOBAL, arg);    
        }
    } else {
        emitBytes(OP_GET_GLOBAL, arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
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
            incrementRule(operatorType, canAssign, false);
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
    [TOKEN_PAGKAKAKILANLAN]  = {variable,  NULL,      PREC_NONE},
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

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);

        if (canAssign && match(TOKEN_KATUMBAS)) {
            error("Mali ang itinuturong lalagyan ng halaga.");
        }
    }
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_PAGKAKAKILANLAN, errorMessage);
    return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void varDeclaration() {
    uint8_t global = parseVariable(
        "Inasahan ang pangalan para sa lalagyan ng nilalaman.");

    if (match(TOKEN_KATUMBAS)) {
        expression();
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

static void printStatement() {
    expression();
    consume(TOKEN_TULDOK_KUWIT, 
        "Inasahan na makakita ng ';' pagtapos ng nilalaman.");
    emitByte(OP_PRINT);
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
    if (match(TOKEN_KILALANIN)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_IPAKITA)) {
        printStatement();
    } else {
        expressionStatement();
    }
}

bool compile(const char* source, Chunk* chunk) {
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    
    while (!match(TOKEN_DULO)) {
        declaration();
    }

    endCompiler();
    return !parser.hadError;
}