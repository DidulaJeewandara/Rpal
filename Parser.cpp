#include "Parser.h"
#include <stdexcept>

Parser::Parser(Lexer& l) : lex(l) {}

// ---------- helpers ----------

bool Parser::isTok(TokType t, const std::string& v) {
    const Token& tk = lex.peek();
    return tk.type == t && tk.value == v;
}

bool Parser::isVal(const std::string& v) {
    return lex.peek().value == v && lex.peek().type != TokType::END;
}

void Parser::read(const std::string& v) {
    const Token& tk = lex.peek();
    if (tk.value != v) {
        throw std::runtime_error("Expected '" + v + "' but found '" + tk.value +
                                 "' at line " + std::to_string(tk.line));
    }
    lex.next();
}

void Parser::readType(TokType t) {
    if (lex.peek().type != t) {
        throw std::runtime_error("Token type mismatch at line " +
                                 std::to_string(lex.peek().line));
    }
    lex.next();
}

Node* Parser::makeLeaf(const std::string& label) {
    return new Node(label);
}

// Pop n nodes, chain them as siblings (in order of grammar), attach as children
// of a new node labeled `label`, push it.
void Parser::buildTree(const std::string& label, int n) {
    Node* result = new Node(label);
    Node* curr = nullptr;
    for (int i = 0; i < n; i++) {
        Node* top = stk.top(); stk.pop();
        top->sibling = curr;
        curr = top;
    }
    result->child = curr;
    stk.push(result);
}

// ---------- entry ----------

Node* Parser::parse() {
    E();
    if (lex.peek().type != TokType::END) {
        throw std::runtime_error("Extra input after expression at line " +
                                 std::to_string(lex.peek().line));
    }
    Node* root = stk.top(); stk.pop();
    return root;
}

// ---------- E / Ew ----------

void Parser::E() {
    if (isTok(TokType::KEYWORD, "let")) {
        lex.next();
        D();
        read("in");
        E();
        buildTree("let", 2);
    } else if (isTok(TokType::KEYWORD, "fn")) {
        lex.next();
        int n = 0;
        while (lex.peek().type == TokType::IDENTIFIER ||
               isTok(TokType::PUNCT, "(")) {
            Vb();
            n++;
        }
        if (n == 0) {
            throw std::runtime_error("Expected variable binding after 'fn' at line " +
                                     std::to_string(lex.peek().line));
        }
        read(".");
        E();
        buildTree("lambda", n + 1);
    } else {
        Ew();
    }
}

void Parser::Ew() {
    T();
    if (isTok(TokType::KEYWORD, "where")) {
        lex.next();
        Dr();
        buildTree("where", 2);
    }
}

// ---------- Tuple ----------

void Parser::T() {
    Ta();
    int n = 1;
    while (isTok(TokType::PUNCT, ",")) {
        lex.next();
        Ta();
        n++;
    }
    if (n > 1) buildTree("tau", n);
}

void Parser::Ta() {
    Tc();
    while (isTok(TokType::KEYWORD, "aug")) {
        lex.next();
        Tc();
        buildTree("aug", 2);
    }
}

void Parser::Tc() {
    B();
    if (isTok(TokType::OPERATOR, "->")) {
        lex.next();
        Tc();
        read("|");
        Tc();
        buildTree("->", 3);
    }
}

// ---------- Boolean ----------

void Parser::B() {
    Bt();
    while (isTok(TokType::KEYWORD, "or")) {
        lex.next();
        Bt();
        buildTree("or", 2);
    }
}

void Parser::Bt() {
    Bs();
    while (isTok(TokType::OPERATOR, "&")) {
        lex.next();
        Bs();
        buildTree("&", 2);
    }
}

void Parser::Bs() {
    if (isTok(TokType::KEYWORD, "not")) {
        lex.next();
        Bp();
        buildTree("not", 1);
    } else {
        Bp();
    }
}

void Parser::Bp() {
    A();
    if (isTok(TokType::KEYWORD, "gr") || isTok(TokType::OPERATOR, ">")) {
        lex.next(); A(); buildTree("gr", 2);
    } else if (isTok(TokType::KEYWORD, "ge") || isTok(TokType::OPERATOR, ">=")) {
        lex.next(); A(); buildTree("ge", 2);
    } else if (isTok(TokType::KEYWORD, "ls") || isTok(TokType::OPERATOR, "<")) {
        lex.next(); A(); buildTree("ls", 2);
    } else if (isTok(TokType::KEYWORD, "le") || isTok(TokType::OPERATOR, "<=")) {
        lex.next(); A(); buildTree("le", 2);
    } else if (isTok(TokType::KEYWORD, "eq")) {
        lex.next(); A(); buildTree("eq", 2);
    } else if (isTok(TokType::KEYWORD, "ne")) {
        lex.next(); A(); buildTree("ne", 2);
    }
}

// ---------- Arithmetic ----------

void Parser::A() {
    if (isTok(TokType::OPERATOR, "+")) {
        lex.next();
        At();
    } else if (isTok(TokType::OPERATOR, "-")) {
        lex.next();
        At();
        buildTree("neg", 1);
    } else {
        At();
    }
    while (isTok(TokType::OPERATOR, "+") || isTok(TokType::OPERATOR, "-")) {
        std::string op = lex.peek().value;
        lex.next();
        At();
        buildTree(op, 2);
    }
}

void Parser::At() {
    Af();
    while (isTok(TokType::OPERATOR, "*") || isTok(TokType::OPERATOR, "/")) {
        std::string op = lex.peek().value;
        lex.next();
        Af();
        buildTree(op, 2);
    }
}

void Parser::Af() {
    Ap();
    if (isTok(TokType::OPERATOR, "**")) {
        lex.next();
        Af();
        buildTree("**", 2);
    }
}

void Parser::Ap() {
    R();
    while (isTok(TokType::OPERATOR, "@")) {
        lex.next();
        const Token& id = lex.peek();
        if (id.type != TokType::IDENTIFIER) {
            throw std::runtime_error("Expected identifier after '@' at line " +
                                     std::to_string(id.line));
        }
        stk.push(makeLeaf("<ID:" + id.value + ">"));
        lex.next();
        R();
        buildTree("@", 3);
    }
}

// ---------- Rator / Rand ----------

static bool startsRn(const Token& t) {
    if (t.type == TokType::IDENTIFIER) return true;
    if (t.type == TokType::INTEGER) return true;
    if (t.type == TokType::STRING) return true;
    if (t.type == TokType::KEYWORD) {
        return t.value == "true" || t.value == "false" ||
               t.value == "nil"  || t.value == "dummy";
    }
    if (t.type == TokType::PUNCT && t.value == "(") return true;
    return false;
}

void Parser::R() {
    Rn();
    while (startsRn(lex.peek())) {
        Rn();
        buildTree("gamma", 2);
    }
}

// Re-escape a string value for the AST <STR:'...'> form.
static std::string escapeString(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'"; break;
            default: out += c; break;
        }
    }
    return out;
}

void Parser::Rn() {
    const Token& t = lex.peek();
    if (t.type == TokType::IDENTIFIER) {
        stk.push(makeLeaf("<ID:" + t.value + ">"));
        lex.next();
    } else if (t.type == TokType::INTEGER) {
        stk.push(makeLeaf("<INT:" + t.value + ">"));
        lex.next();
    } else if (t.type == TokType::STRING) {
        stk.push(makeLeaf("<STR:'" + escapeString(t.value) + "'>"));
        lex.next();
    } else if (t.type == TokType::KEYWORD && t.value == "true") {
        stk.push(makeLeaf("<true>")); lex.next();
    } else if (t.type == TokType::KEYWORD && t.value == "false") {
        stk.push(makeLeaf("<false>")); lex.next();
    } else if (t.type == TokType::KEYWORD && t.value == "nil") {
        stk.push(makeLeaf("<nil>")); lex.next();
    } else if (t.type == TokType::KEYWORD && t.value == "dummy") {
        stk.push(makeLeaf("<dummy>")); lex.next();
    } else if (t.type == TokType::PUNCT && t.value == "(") {
        lex.next();
        E();
        read(")");
    } else {
        throw std::runtime_error("Unexpected token '" + t.value +
                                 "' at line " + std::to_string(t.line));
    }
}

// ---------- Definitions ----------

void Parser::D() {
    Da();
    if (isTok(TokType::KEYWORD, "within")) {
        lex.next();
        D();
        buildTree("within", 2);
    }
}

void Parser::Da() {
    Dr();
    int n = 1;
    while (isTok(TokType::KEYWORD, "and")) {
        lex.next();
        Dr();
        n++;
    }
    if (n > 1) buildTree("and", n);
}

void Parser::Dr() {
    if (isTok(TokType::KEYWORD, "rec")) {
        lex.next();
        Db();
        buildTree("rec", 1);
    } else {
        Db();
    }
}

void Parser::Db() {
    const Token& t = lex.peek();
    if (t.type == TokType::PUNCT && t.value == "(") {
        lex.next();
        D();
        read(")");
    } else if (t.type == TokType::IDENTIFIER) {
        // Could be Vl '=' E (single or comma-list) or function_form
        stk.push(makeLeaf("<ID:" + t.value + ">"));
        lex.next();
        if (isTok(TokType::PUNCT, ",")) {
            // Vl with comma list, then '=' E
            int n = 1;
            while (isTok(TokType::PUNCT, ",")) {
                lex.next();
                const Token& id = lex.peek();
                if (id.type != TokType::IDENTIFIER) {
                    throw std::runtime_error("Expected identifier after ',' at line " +
                                             std::to_string(id.line));
                }
                stk.push(makeLeaf("<ID:" + id.value + ">"));
                lex.next();
                n++;
            }
            buildTree(",", n);
            read("=");
            E();
            buildTree("=", 2);
        } else if (isTok(TokType::OPERATOR, "=")) {
            lex.next();
            E();
            buildTree("=", 2);
        } else {
            // function_form: name is on stack; parse Vb+
            int n = 0;
            while (lex.peek().type == TokType::IDENTIFIER ||
                   isTok(TokType::PUNCT, "(")) {
                Vb();
                n++;
            }
            if (n == 0) {
                throw std::runtime_error("Expected '=' or variable binding at line " +
                                         std::to_string(lex.peek().line));
            }
            read("=");
            E();
            buildTree("function_form", n + 2);
        }
    } else {
        throw std::runtime_error("Expected definition at line " +
                                 std::to_string(t.line));
    }
}

// ---------- Variables ----------

void Parser::Vb() {
    const Token& t = lex.peek();
    if (t.type == TokType::IDENTIFIER) {
        stk.push(makeLeaf("<ID:" + t.value + ">"));
        lex.next();
    } else if (t.type == TokType::PUNCT && t.value == "(") {
        lex.next();
        if (isTok(TokType::PUNCT, ")")) {
            lex.next();
            buildTree("()", 0);
        } else {
            Vl();
            read(")");
        }
    } else {
        throw std::runtime_error("Expected variable binding at line " +
                                 std::to_string(t.line));
    }
}

int Parser::Vl() {
    const Token& t = lex.peek();
    if (t.type != TokType::IDENTIFIER) {
        throw std::runtime_error("Expected identifier at line " +
                                 std::to_string(t.line));
    }
    stk.push(makeLeaf("<ID:" + t.value + ">"));
    lex.next();
    int n = 1;
    while (isTok(TokType::PUNCT, ",")) {
        lex.next();
        const Token& id = lex.peek();
        if (id.type != TokType::IDENTIFIER) {
            throw std::runtime_error("Expected identifier after ',' at line " +
                                     std::to_string(id.line));
        }
        stk.push(makeLeaf("<ID:" + id.value + ">"));
        lex.next();
        n++;
    }
    if (n > 1) buildTree(",", n);
    return n;
}
