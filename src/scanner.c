#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

#define TAB_SPACE 4

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static char previous() {
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_PROBLEMA;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

static void skipWhiteSpace() {
    for (;;) {
        char c = peek();
        
        switch (c) {
            case ' ':
            case '\r':
            case '\t': {
                advance();
                break;
            }
            case '\n':
                advance();
                scanner.line++;
                break;
            case '/':
                if (peekNext() == '/') {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return; 
                }
                break;
            default:
                return; 
        }
    }
}

static TokenType checkKeyword(int start, int length,
        const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {

        return type;
    }

    return TOKEN_PAGKAKAKILANLAN;
}

static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'a': return checkKeyword(1, 1, "t", TOKEN_AT);
        case 'g':
            if (scanner.current - scanner.start > 4) {
                switch (scanner.start[3]) {
                    case 'a': return checkKeyword(1, 5, "awain", TOKEN_GAWAIN);
                    case 'i': return checkKeyword(1, 4, "awin", TOKEN_GAWIN);
                }
            }
            break;
        case 'h': return checkKeyword(1, 5, "abang", TOKEN_HABANG);
        case 'i':
            if (scanner.current - scanner.start > 2) {
                switch (scanner.start[1]) {
                    case 'b': return checkKeyword(2, 4, "alik", TOKEN_IBALIK);
                    case 'p': return checkKeyword(2, 5, "akita", TOKEN_IPAKITA);
                    case 't':
                        switch (scanner.start[2]) {
                            case 'i': return checkKeyword(3, 3, "gil", TOKEN_ITIGIL);
                            case 'o': return TOKEN_ITO;
                            case 'u': return checkKeyword(3, 3, "loy", TOKEN_ITULOY);
                        }
                        break;
                }
            }
            break;
        case 'k':
            if (scanner.current - scanner.start > 3) {
                switch (scanner.start[1]) {
                    case 'a': {
                        switch (scanner.start[2]) {
                            case 'd': return checkKeyword(3, 1, "a", TOKEN_KADA);
                            case 'p': return checkKeyword(3, 2, "ag", TOKEN_KAPAG);
                        }
                    }
                    case 'i': return checkKeyword(2, 7, "lalanin", TOKEN_KILALANIN);
                    case 'u': 
                        switch (scanner.start[3]) {
                            case 'd': return checkKeyword(2, 6, "ndiman", TOKEN_KUNDIMAN);
                            case 'g': return checkKeyword(2, 2, "ng", TOKEN_KUNG);
                        }
                        break;
                }
            }
            break;
        case 'm':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(2, 2, "li", TOKEN_MALI);
                    case 'u': return checkKeyword(2, 2, "la", TOKEN_MULA);
                }
            }
        case 'n': return checkKeyword(1, 3, "ull", TOKEN_NULL);
        case 'o': return checkKeyword(1, 0, "o", TOKEN_O);
        case 'p': return checkKeyword(1, 4, "alya", TOKEN_PALYA);
        case 's': return checkKeyword(1, 5, "uriin", TOKEN_SURIIN);
        case 't': return checkKeyword(1, 3, "ama", TOKEN_TAMA);
        case 'u': return checkKeyword(1, 2, "ri", TOKEN_URI);
    }

    return TOKEN_PAGKAKAKILANLAN;
}

static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

static Token number() {
    while (isDigit(peek())) advance();

    // Look for a fractional part.
    if (peek() == '.' && isDigit(peekNext())) {
        // Consume the ".".
        advance();

        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMERO);
}

static Token string() {
    while (!isAtEnd()) {
        if (peek() == '"' && previous() != '\\') break;
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Walang panapos na \" ang grupo ng mga titik.");

    // The closing quote.
    advance();
    return makeToken(TOKEN_SALITA);
}

Token scanToken() {
    skipWhiteSpace();
    scanner.start = scanner.current;

    if (isAtEnd()) return makeToken(TOKEN_DULO);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case '(':  return makeToken(TOKEN_KALIWANG_PAREN);
        case ')':  return makeToken(TOKEN_KANANG_PAREN);
        case '{':  return makeToken(TOKEN_KALIWANG_BRACE);
        case '}':  return makeToken(TOKEN_KANANG_BRACE);
        case '[':  return makeToken(TOKEN_KALIWANG_BRACKET);
        case ']':  return makeToken(TOKEN_KANANG_BRACKET);
        case ':':  return makeToken(TOKEN_TUTULDOK);
        case ';':  return makeToken(TOKEN_TULDOK_KUWIT);
        case ',':  return makeToken(TOKEN_KUWIT);
        case '.':  return makeToken(TOKEN_TULDOK);
        case '\\': return makeToken(TOKEN_ATRAS_PAHILIS);
        case '/':  return makeToken(TOKEN_SULONG_PAHILIS);
        case '*':  return makeToken(TOKEN_BITUIN);
        case '%':  return makeToken(TOKEN_MODULO);
        case '-': 
            return makeToken(
                match('-') ? TOKEN_BAWAS_ISA : TOKEN_BAWAS);
        case '+': 
            return makeToken(
                match('+') ? TOKEN_DAGDAG_ISA : TOKEN_DAGDAG);
        case '!':
            return makeToken(
                match('=') ? TOKEN_HINDI_PAREHO : TOKEN_HINDI);
        case '=':
            return makeToken(
                match('=') ? TOKEN_PAREHO : TOKEN_KATUMBAS);
        case '>':
            return makeToken(
                match('=') ? TOKEN_HIGIT_PAREHO : TOKEN_HIGIT);
        case '<':
            return makeToken(
                match('=') ? TOKEN_BABA_PAREHO : TOKEN_BABA);
        case '"': return string();
    }

    return errorToken("Hindi kilalang simbolo.");
}
