#ifndef CSEMACHINE_H
#define CSEMACHINE_H

#include "Node.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <ostream>

// ---- Lambda variable spec (the parameter binding of a lambda) ----
struct LambdaVar {
    enum Kind { SINGLE, TUPLE, EMPTY };
    Kind kind;
    std::vector<std::string> names;
    LambdaVar() : kind(EMPTY) {}
};

// ---- Environment ----
struct Environment;
using EnvPtr = Environment*;

// ---- Value (operand stack item) ----
struct Value {
    enum Kind {
        INT, STR, BOOL, NIL, DUMMY, TUPLE,
        CLOSURE, ETA_CLOSURE, YSTAR,
        BUILTIN, PARTIAL_BUILTIN,
        ENV_MARKER_V    // placeholder if needed
    };
    Kind kind;

    long long intVal = 0;
    std::string strVal;
    bool boolVal = false;
    std::vector<Value> tupleVals;

    // Closure
    int closureK = -1;
    LambdaVar closureVar;
    EnvPtr closureEnv = nullptr;

    // Built-in
    std::string builtinName;
    std::vector<Value> partialArgs;

    Value() : kind(DUMMY) {}
};

// ---- Environment ----
struct Environment {
    int index;
    std::unordered_map<std::string, Value> bindings;
    EnvPtr parent;
    Environment(int idx, EnvPtr par) : index(idx), parent(par) {}
    Value lookup(const std::string& name) const;
    bool has(const std::string& name) const;
};

// ---- Control structure item ----
struct Item {
    enum Kind {
        NAME, INT_LIT, STR_LIT, TRUE_LIT, FALSE_LIT, NIL_LIT, DUMMY_LIT, YSTAR_LIT,
        LAMBDA, GAMMA, BETA, DELTA_REF, TAU, OP, ENV_MARKER,
        LITERAL_VAL    // synthetic: push embedded value onto stack (used by eta-app rewriting)
    };
    Kind kind;
    std::string sval;     // NAME, STR_LIT, OP
    long long ival = 0;   // INT_LIT
    int deltaIdx = -1;    // LAMBDA, DELTA_REF
    LambdaVar var;        // LAMBDA
    int n = 0;            // TAU
    EnvPtr envPtr = nullptr; // ENV_MARKER
    Value litVal;         // LITERAL_VAL
};

// ---- CSE Machine ----
class CSEMachine {
public:
    explicit CSEMachine(Node* stRoot);
    ~CSEMachine();

    // Run the program, emit any Print output to `out`.
    void run(std::ostream& out);

private:
    Node* root;
    std::vector<std::vector<Item>> deltas;   // delta_0, delta_1, ...
    std::vector<Item> control;               // top = back()
    std::vector<Value> stack;                // top = back()
    std::vector<EnvPtr> allEnvs;             // owns environments
    EnvPtr currentEnv = nullptr;
    int nextEnvIdx = 0;
    std::ostream* out_ = nullptr;

    // Build control structures from ST.
    int flatten(Node* node);                 // returns the delta index created
    void flattenInto(Node* node, std::vector<Item>& cur);

    // Helpers for tree inspection.
    static LambdaVar extractVar(Node* v);

    // Set up initial environment with built-ins.
    void initPrimitives();

    // Apply gamma: rator on top, rand below. Pops both.
    void doGamma();

    // Push items of delta_k onto control in reverse so delta_k[0] is on top.
    void pushDelta(int k);

    // Operators (binary/unary).
    void doOp(const std::string& op);

    // Built-in application; called once we know rator is BUILTIN or PARTIAL_BUILTIN.
    void applyBuiltin(Value rator, Value rand);

    // Format a value for Print output.
    std::string formatValue(const Value& v);

    // Helper: bind LambdaVar to argument value into env.
    void bindArg(EnvPtr env, const LambdaVar& var, const Value& arg);
};

#endif
