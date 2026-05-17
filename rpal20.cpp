// rpal20: lexer, parser, AST -> ST standardizer, CSE machine for RPAL.
// Usage:
//   rpal20 <file>            evaluate the program
//   rpal20 -ast <file>       print AST only
//   rpal20 -st  <file>       print Standardized Tree only

#include "Lexer.h"
#include "Parser.h"
#include "Standardizer.h"
#include "CSEMachine.h"
#include "Node.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [-ast | -st] <file>\n";
        return 1;
    }

    bool printAst = false;
    bool printSt  = false;
    std::string path;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-ast") printAst = true;
        else if (a == "-st") printSt = true;
        else path = a;
    }
    if (path.empty()) {
        std::cerr << "No input file specified.\n";
        return 1;
    }

    try {
        std::string src = readFile(path);
        Lexer lex(src);
        Parser parser(lex);
        Node* ast = parser.parse();

        if (printAst) {
            printTree(ast, std::cout);
            freeTree(ast);
            return 0;
        }

        Node* st = Standardizer::standardize(ast);

        if (printSt) {
            printTree(st, std::cout);
            freeTree(st);
            return 0;
        }

        CSEMachine cse(st);
        cse.run(std::cout);
        // Most rpal.exe outputs end with a newline.
        std::cout << "\n";
        freeTree(st);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
