#include "Standardizer.h"
#include <vector>

// Deep clone a subtree (children and siblings).
static Node* cloneTree(Node* n) {
    if (!n) return nullptr;
    Node* c = new Node(n->label);
    c->child = cloneTree(n->child);
    c->sibling = cloneTree(n->sibling);
    return c;
}

// let(=(X,E), P) -> gamma(lambda(X,P), E)
static Node* stdLet(Node* n) {
    Node* eq = n->child;
    Node* P  = eq->sibling;
    Node* X  = eq->child;
    Node* E  = X->sibling;
    X->sibling = nullptr; P->sibling = nullptr; E->sibling = nullptr;

    Node* lam = new Node("lambda");
    lam->child = X; X->sibling = P;
    Node* gamma = new Node("gamma");
    gamma->child = lam; lam->sibling = E;
    return gamma;
}

// where(P, =(X,E)) -> gamma(lambda(X,P), E)
static Node* stdWhere(Node* n) {
    Node* P  = n->child;
    Node* eq = P->sibling;
    Node* X  = eq->child;
    Node* E  = X->sibling;
    X->sibling = nullptr; P->sibling = nullptr; E->sibling = nullptr;

    Node* lam = new Node("lambda");
    lam->child = X; X->sibling = P;
    Node* gamma = new Node("gamma");
    gamma->child = lam; lam->sibling = E;
    return gamma;
}

// function_form(f, V1, ..., Vn, E) -> =(f, lambda(V1, lambda(V2, ..., lambda(Vn, E))))
static Node* stdFcnForm(Node* n) {
    std::vector<Node*> kids;
    for (Node* c = n->child; c; c = c->sibling) kids.push_back(c);
    for (Node* c : kids) c->sibling = nullptr;

    Node* f    = kids.front();
    Node* body = kids.back();
    Node* inner = body;
    for (int i = (int)kids.size() - 2; i >= 1; i--) {
        Node* lam = new Node("lambda");
        lam->child = kids[i]; kids[i]->sibling = inner;
        inner = lam;
    }
    Node* eq = new Node("=");
    eq->child = f; f->sibling = inner;
    return eq;
}

// lambda(V1, V2, ..., Vn, E) -> lambda(V1, lambda(V2, ..., lambda(Vn, E)))
// After this, every lambda has exactly 2 children.
static Node* stdLambda(Node* n) {
    std::vector<Node*> kids;
    for (Node* c = n->child; c; c = c->sibling) kids.push_back(c);
    if (kids.size() == 2) return n;
    for (Node* c : kids) c->sibling = nullptr;

    Node* body = kids.back();
    Node* inner = body;
    for (int i = (int)kids.size() - 2; i >= 1; i--) {
        Node* lam = new Node("lambda");
        lam->child = kids[i]; kids[i]->sibling = inner;
        inner = lam;
    }
    n->child = kids[0];
    kids[0]->sibling = inner;
    return n;
}

// within(=(X1,E1), =(X2,E2)) -> =(X2, gamma(lambda(X1,E2), E1))
static Node* stdWithin(Node* n) {
    Node* eq1 = n->child;
    Node* eq2 = eq1->sibling;
    Node* X1 = eq1->child; Node* E1 = X1->sibling;
    Node* X2 = eq2->child; Node* E2 = X2->sibling;
    X1->sibling = nullptr; E1->sibling = nullptr;
    X2->sibling = nullptr; E2->sibling = nullptr;

    Node* lam = new Node("lambda");
    lam->child = X1; X1->sibling = E2;
    Node* gamma = new Node("gamma");
    gamma->child = lam; lam->sibling = E1;
    Node* eq = new Node("=");
    eq->child = X2; X2->sibling = gamma;
    return eq;
}

// @(E1, N, E2) -> gamma(gamma(N, E1), E2)
static Node* stdAt(Node* n) {
    Node* E1 = n->child;
    Node* N  = E1->sibling;
    Node* E2 = N->sibling;
    E1->sibling = nullptr; N->sibling = nullptr; E2->sibling = nullptr;

    Node* inner = new Node("gamma");
    inner->child = N; N->sibling = E1;
    Node* outer = new Node("gamma");
    outer->child = inner; inner->sibling = E2;
    return outer;
}

// and(=(X1,E1), ..., =(Xn,En)) -> =(,(X1, ..., Xn), tau(E1, ..., En))
static Node* stdAnd(Node* n) {
    std::vector<Node*> Xs, Es;
    for (Node* eq = n->child; eq; eq = eq->sibling) {
        Node* X = eq->child;
        Node* E = X->sibling;
        Xs.push_back(X); Es.push_back(E);
    }
    for (auto* x : Xs) x->sibling = nullptr;
    for (auto* e : Es) e->sibling = nullptr;

    Node* comma = new Node(",");
    Node* prev = nullptr;
    for (auto* x : Xs) {
        if (!prev) comma->child = x; else prev->sibling = x;
        prev = x;
    }
    Node* tau = new Node("tau");
    prev = nullptr;
    for (auto* e : Es) {
        if (!prev) tau->child = e; else prev->sibling = e;
        prev = e;
    }
    Node* eq = new Node("=");
    eq->child = comma; comma->sibling = tau;
    return eq;
}

// rec(=(X, E)) -> =(X, gamma(<Y*>, lambda(X, E)))
static Node* stdRec(Node* n) {
    Node* eq = n->child;
    Node* X  = eq->child;
    Node* E  = X->sibling;
    X->sibling = nullptr; E->sibling = nullptr;

    Node* Xclone = cloneTree(X);

    Node* lam = new Node("lambda");
    lam->child = X; X->sibling = E;

    Node* Ystar = new Node("<Y*>");
    Node* gamma = new Node("gamma");
    gamma->child = Ystar; Ystar->sibling = lam;

    Node* eqNew = new Node("=");
    eqNew->child = Xclone; Xclone->sibling = gamma;
    return eqNew;
}

Node* Standardizer::standardize(Node* n) {
    if (!n) return nullptr;

    // post-order: standardize all children first
    Node* prev = nullptr;
    Node* c = n->child;
    while (c) {
        Node* nextSib = c->sibling;
        c->sibling = nullptr;
        Node* newC = standardize(c);
        newC->sibling = nextSib;
        if (prev) prev->sibling = newC;
        else      n->child = newC;
        prev = newC;
        c = nextSib;
    }

    const std::string& L = n->label;
    if (L == "let")           return stdLet(n);
    if (L == "where")         return stdWhere(n);
    if (L == "function_form") return stdFcnForm(n);
    if (L == "lambda")        return stdLambda(n);
    if (L == "within")        return stdWithin(n);
    if (L == "@")             return stdAt(n);
    if (L == "and")           return stdAnd(n);
    if (L == "rec")           return stdRec(n);
    return n;
}
