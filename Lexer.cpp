#include "Lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_set>

static const std::unordered_set<std::string> KEYWORDS = {
    "let", "in", "where", "fn", "aug", "or", "not",
    "gr", "ge", "ls", "le", "eq", "ne",
    "true", "false", "nil", "dummy",
    "within", "and", "rec"
};

Lexer::Lexer(std::string s)
    : src(std::move(s)), pos(0), line(1), hasCached(false) {}

bool Lexer::isLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool Lexer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::isOperatorSymbol(char c) {
    // Per RPAL_Lex.pdf: + - * < > & . @ / : = ~ | $ ! # % ^ _ [ ] { } " ` ?
    switch (c) {
        case '+': case '-': case '*': case '<': case '>': case '&':
        case '.': case '@': case '/': case ':': case '=': case '~':
        case '|': case '$': case '!': case '#': case '%': case '^':
        case '_': case '[': case ']': case '{': case '}': case '"':
        case '`': case '?':
            return true;
        default:
            return false;
    }
}

bool Lexer::isKeyword(const std::string& s) {
    return KEYWORDS.count(s) > 0;
}

void Lexer::skipSpacesAndComments() {
    while (pos < src.size()) {
        char c = src[pos];
        if (c == ' ' || c == '\t') {
            pos++;
        } else if (c == '\n') {
            pos++;
            line++;
        } else if (c == '\r') {
            pos++;
        } else if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
            // line comment: skip to end of line
            pos += 2;
            while (pos < src.size() && src[pos] != '\n') pos++;
        } else {
            break;
        }
    }
}

Token Lexer::readIdentOrKeyword() {
    int startLine = line;
    size_t start = pos;
    pos++; // first letter already validated
    while (pos < src.size()) {
        char c = src[pos];
        if (isLetter(c) || isDigit(c) || c == '_') pos++;
        else break;
    }
    std::string value = src.substr(start, pos - start);
    if (isKeyword(value)) {
        return Token(TokType::KEYWORD, value, startLine);
    }
    return Token(TokType::IDENTIFIER, value, startLine);
}

Token Lexer::readInteger() {
    int startLine = line;
    size_t start = pos;
    while (pos < src.size() && isDigit(src[pos])) pos++;
    return Token(TokType::INTEGER, src.substr(start, pos - start), startLine);
}

Token Lexer::readString() {
    int startLine = line;
    pos++; // skip opening '
    std::string value;
    while (pos < src.size() && src[pos] != '\'') {
        char c = src[pos];
        if (c == '\\' && pos + 1 < src.size()) {
            char nxt = src[pos + 1];
            switch (nxt) {
                case 't':  value += '\t'; pos += 2; break;
                case 'n':  value += '\n'; pos += 2; break;
                case '\\': value += '\\'; pos += 2; break;
                case '\'': value += '\''; pos += 2; break;
                default:   value += c; pos++; break;
            }
        } else {
            if (c == '\n') line++;
            value += c;
            pos++;
        }
    }
    if (pos >= src.size()) {
        throw std::runtime_error("Unterminated string literal at line " + std::to_string(startLine));
    }
    pos++; // skip closing '
    return Token(TokType::STRING, value, startLine);
}

Token Lexer::readOperator() {
    int startLine = line;
    size_t start = pos;
    while (pos < src.size() && isOperatorSymbol(src[pos])) pos++;
    return Token(TokType::OPERATOR, src.substr(start, pos - start), startLine);
}

Token Lexer::readToken() {
    skipSpacesAndComments();
    if (pos >= src.size()) return Token(TokType::END, "", line);

    char c = src[pos];

    if (isLetter(c)) return readIdentOrKeyword();
    if (isDigit(c))  return readInteger();
    if (c == '\'')   return readString();
    if (c == '(' || c == ')' || c == ';' || c == ',') {
        std::string v(1, c);
        pos++;
        return Token(TokType::PUNCT, v, line);
    }
    if (isOperatorSymbol(c)) return readOperator();

    throw std::runtime_error("Unexpected character '" + std::string(1, c) +
                             "' at line " + std::to_string(line));
}

Token Lexer::next() {
    if (hasCached) {
        hasCached = false;
        return cached;
    }
    return readToken();
}

const Token& Lexer::peek() {
    if (!hasCached) {
        cached = readToken();
        hasCached = true;
    }
    return cached;
}
