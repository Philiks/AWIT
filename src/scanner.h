#ifndef awit_scanner_h
#define awit_scanner_h

typedef enum {
    // Single-character tokens.
    TOKEN_KALIWANG_PAREN, TOKEN_KANANG_PAREN,
    TOKEN_KALIWANG_BRACE, TOKEN_KANANG_BRACE,
    TOKEN_KUWIT, TOKEN_TULDOK, TOKEN_TULDOK_KUWIT, 
    TOKEN_TUTULDOK, TOKEN_PAHILIS, TOKEN_BITUIN, 
    TOKEN_MODULO,
    // One or two character tokens.
    TOKEN_BAWAS, TOKEN_BAWAS_ISA,
    TOKEN_DAGDAG, TOKEN_DAGDAG_ISA,
    TOKEN_HINDI, TOKEN_HINDI_PAREHO,
    TOKEN_KATUMBAS, TOKEN_PAREHO,
    TOKEN_HIGIT, TOKEN_HIGIT_PAREHO,
    TOKEN_BABA, TOKEN_BABA_PAREHO,
    // Literals.
    TOKEN_PAGKAKAKILANLAN, TOKEN_SALITA, TOKEN_NUMERO,
    // Keywords.
    TOKEN_AT, TOKEN_GAWAIN, TOKEN_GAWIN, TOKEN_HABANG,
    TOKEN_IBALIK, TOKEN_IPAKITA, TOKEN_ITIGIL, TOKEN_ITO,
    TOKEN_ITULOY, TOKEN_KADA, TOKEN_KAPAG, TOKEN_KILALANIN, 
    TOKEN_KUNDIMAN, TOKEN_KUNG, TOKEN_MALI, TOKEN_MULA, 
    TOKEN_NULL, TOKEN_PALYA, TOKEN_SIM, TOKEN_SURIIN, 
    TOKEN_O, TOKEN_TAMA, TOKEN_URI,

    TOKEN_PROBLEMA, TOKEN_DULO
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

void initScanner(const char* soure);
Token scanToken();

#endif
