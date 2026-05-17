#ifndef NODE_H
#define NODE_H

#include <string>
#include <ostream>

// AST / ST node, first-child / right-sibling representation.
// Cheap to mutate during standardization.
struct Node {
    std::string label;
    Node* child;
    Node* sibling;
    explicit Node(std::string l) : label(std::move(l)), child(nullptr), sibling(nullptr) {}
};

// Print tree in preorder with leading dots indicating depth (rpal.exe style).
void printTree(Node* n, std::ostream& out);

// Free a tree.
void freeTree(Node* n);

// Count children of a node.
int childCount(Node* n);

// Get child by index (0-based). Returns nullptr if out of range.
Node* childAt(Node* n, int idx);

#endif
