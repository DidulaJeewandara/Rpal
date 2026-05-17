#ifndef PARSER_H
#define PARSER_H

#include "Lexer.h"
#include "Node.h"
#include <stack>
#include <string>

// Recursive-descent parser for RPAL. Builds the AST per RPAL_Grammar.pdf.
class Parser {
public:
    explicit Parser(Lexer& l);

    // Parse the entire input and return the AST root.
    // Throws std::runtime_error on syntax error.
    Node* parse();

private:
    Lexer& lex;
    std::stack<Node*> stk;

    // helpers
    bool isTok(TokType t, const std::string& v);
    bool isVal(const std::string& v);   // value match, any type
    void read(const std::string& v);    // expect a specific value
    void readType(TokType t);
    void buildTree(const std::string& label, int n);
    Node* makeLeaf(const std::string& label);

    // Non-terminals
    void E();   void Ew();
    void T();   void Ta();  void Tc();
    void B();   void Bt();  void Bs();  void Bp();
    void A();   void At();  void Af();  void Ap();
    void R();   void Rn();
    void D();   void Da();  void Dr();  void Db();
    void Vb();
    int  Vl();  // returns count of identifiers
};

#endif
