#ifndef LEXER_H
#define LEXER_H

#include "Token.h"
#include <string>

// Lexical analyzer for RPAL. Reads source text and yields tokens
// per RPAL_Lex.pdf rules.
class Lexer {
public:
    explicit Lexer(std::string src);

    // Consume and return the next token (or END).
    Token next();

    // Return the next token without consuming.
    const Token& peek();

private:
    std::string src;
    size_t pos;
    int line;
    Token cached;
    bool hasCached;

    void skipSpacesAndComments();
    Token readToken();
    Token readIdentOrKeyword();
    Token readInteger();
    Token readString();
    Token readOperator();

    static bool isLetter(char c);
    static bool isDigit(char c);
    static bool isOperatorSymbol(char c);
    static bool isKeyword(const std::string& s);
};

#endif
