#ifndef STANDARDIZER_H
#define STANDARDIZER_H

#include "Node.h"

// Converts an AST into a Standardized Tree (ST) by applying RPAL's
// standardization rules recursively in post-order.
class Standardizer {
public:
    // Returns the (possibly new) root of the standardized subtree.
    static Node* standardize(Node* n);
};

#endif
