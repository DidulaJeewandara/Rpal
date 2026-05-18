#include "Node.h"
#include <string>

static std::string displayLabel(const std::string& label) {
    // Strip token-wrapper tags so the AST prints "Print" instead of "<ID:Print>",
    // "5" instead of "<INT:5>", and "'hello'" instead of "<STR:'hello'>".
    auto stripped = [&](const char* prefix) -> std::string {
        size_t pn = std::string(prefix).size();
        if (label.size() >= pn + 1 &&
            label.compare(0, pn, prefix) == 0 &&
            label.back() == '>') {
            return label.substr(pn, label.size() - pn - 1);
        }
        return {};
    };
    std::string s;
    if (!(s = stripped("<ID:")).empty()  ||
        !(s = stripped("<INT:")).empty() ||
        !(s = stripped("<STR:")).empty()) {
        return s;
    }
    return label;
}

void printTree(Node* n, std::ostream& out) {
    // recursive helper using an explicit depth counter
    // walk first-child / right-sibling
    struct Walker {
        std::ostream& out;
        void go(Node* node, int depth) {
            if (!node) return;
            for (int i = 0; i < depth; i++) out.put('.');
            out << displayLabel(node->label) << '\n';
            go(node->child, depth + 1);
            go(node->sibling, depth);
        }
    } w{out};
    w.go(n, 0);
}

void freeTree(Node* n) {
    if (!n) return;
    freeTree(n->child);
    freeTree(n->sibling);
    delete n;
}

int childCount(Node* n) {
    if (!n) return 0;
    int c = 0;
    for (Node* k = n->child; k; k = k->sibling) c++;
    return c;
}

Node* childAt(Node* n, int idx) {
    if (!n) return nullptr;
    Node* k = n->child;
    while (k && idx > 0) { k = k->sibling; idx--; }
    return k;
}
