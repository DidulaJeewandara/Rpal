#ifndef TOKEN_H
#define TOKEN_H

#include <string>

enum class TokType {
    IDENTIFIER,
    INTEGER,
    STRING,
    OPERATOR,
    KEYWORD,
    PUNCT,
    END
};

struct Token {
    TokType type;
    std::string value;
    int line;
    Token() : type(TokType::END), value(""), line(0) {}
    Token(TokType t, std::string v, int l) : type(t), value(std::move(v)), line(l) {}
};

#endif
