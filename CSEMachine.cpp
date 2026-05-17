#include "CSEMachine.h"
#include <stdexcept>
#include <sstream>
#include <cstdlib>

// ============================================================
// Environment
// ============================================================

Value Environment::lookup(const std::string& name) const {
    auto it = bindings.find(name);
    if (it != bindings.end()) return it->second;
    if (parent) return parent->lookup(name);
    throw std::runtime_error("Undefined name: " + name);
}

bool Environment::has(const std::string& name) const {
    if (bindings.count(name)) return true;
    if (parent) return parent->has(name);
    return false;
}

// ============================================================
// Helpers to parse leaf labels
// ============================================================

// label like "<ID:name>" returns "name"
static bool isPrefixed(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static std::string stripWrap(const std::string& s, size_t prefixLen, size_t suffixLen) {
    return s.substr(prefixLen, s.size() - prefixLen - suffixLen);
}

// Unescape RPAL string contents: \n \t \\ \'
static std::string unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char nxt = s[i + 1];
            switch (nxt) {
                case 'n':  out += '\n'; i++; break;
                case 't':  out += '\t'; i++; break;
                case '\\': out += '\\'; i++; break;
                case '\'': out += '\''; i++; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// ============================================================
// LambdaVar extraction from a node
// ============================================================
LambdaVar CSEMachine::extractVar(Node* v) {
    LambdaVar lv;
    if (!v) { lv.kind = LambdaVar::EMPTY; return lv; }
    if (v->label == "()") {
        lv.kind = LambdaVar::EMPTY;
        return lv;
    }
    if (isPrefixed(v->label, "<ID:")) {
        lv.kind = LambdaVar::SINGLE;
        lv.names.push_back(stripWrap(v->label, 4, 1));
        return lv;
    }
    if (v->label == ",") {
        lv.kind = LambdaVar::TUPLE;
        for (Node* c = v->child; c; c = c->sibling) {
            if (isPrefixed(c->label, "<ID:")) {
                lv.names.push_back(stripWrap(c->label, 4, 1));
            }
        }
        return lv;
    }
    lv.kind = LambdaVar::EMPTY;
    return lv;
}

// ============================================================
// CSEMachine ctor / dtor
// ============================================================
CSEMachine::CSEMachine(Node* stRoot) : root(stRoot) {}

CSEMachine::~CSEMachine() {
    for (auto* e : allEnvs) delete e;
}

// ============================================================
// Flatten ST into control structures
// ============================================================

void CSEMachine::flattenInto(Node* n, std::vector<Item>& cur) {
    if (!n) return;
    const std::string& L = n->label;

    // Leaf literals
    if (isPrefixed(L, "<ID:")) {
        Item it; it.kind = Item::NAME; it.sval = stripWrap(L, 4, 1);
        cur.push_back(it); return;
    }
    if (isPrefixed(L, "<INT:")) {
        Item it; it.kind = Item::INT_LIT;
        it.ival = std::strtoll(stripWrap(L, 5, 1).c_str(), nullptr, 10);
        cur.push_back(it); return;
    }
    if (isPrefixed(L, "<STR:")) {
        // strip "<STR:'" and "'>"
        std::string raw = L.substr(6, L.size() - 8);
        Item it; it.kind = Item::STR_LIT; it.sval = unescape(raw);
        cur.push_back(it); return;
    }
    if (L == "<true>")  { Item it; it.kind = Item::TRUE_LIT;  cur.push_back(it); return; }
    if (L == "<false>") { Item it; it.kind = Item::FALSE_LIT; cur.push_back(it); return; }
    if (L == "<nil>")   { Item it; it.kind = Item::NIL_LIT;   cur.push_back(it); return; }
    if (L == "<dummy>") { Item it; it.kind = Item::DUMMY_LIT; cur.push_back(it); return; }
    if (L == "<Y*>")    { Item it; it.kind = Item::YSTAR_LIT; cur.push_back(it); return; }

    if (L == "lambda") {
        Node* V    = n->child;
        Node* body = V ? V->sibling : nullptr;
        int k = (int)deltas.size();
        deltas.emplace_back();           // reserve index
        std::vector<Item> bodyDelta;
        flattenInto(body, bodyDelta);
        deltas[k] = std::move(bodyDelta);
        Item it; it.kind = Item::LAMBDA;
        it.deltaIdx = k;
        it.var = extractVar(V);
        cur.push_back(it);
        return;
    }

    if (L == "->") {
        Node* cond = n->child;
        Node* th   = cond->sibling;
        Node* el   = th->sibling;
        int kt = (int)deltas.size();
        deltas.emplace_back();
        std::vector<Item> thenDelta;
        flattenInto(th, thenDelta);
        deltas[kt] = std::move(thenDelta);
        int ke = (int)deltas.size();
        deltas.emplace_back();
        std::vector<Item> elseDelta;
        flattenInto(el, elseDelta);
        deltas[ke] = std::move(elseDelta);

        // Emit in execution order: cond, then Beta, then DR_then, DR_else.
        flattenInto(cond, cur);
        Item bt; bt.kind = Item::BETA; cur.push_back(bt);
        Item drt; drt.kind = Item::DELTA_REF; drt.deltaIdx = kt; cur.push_back(drt);
        Item dre; dre.kind = Item::DELTA_REF; dre.deltaIdx = ke; cur.push_back(dre);
        return;
    }

    if (L == "tau") {
        int n_children = 0;
        for (Node* c = n->child; c; c = c->sibling) {
            flattenInto(c, cur); n_children++;
        }
        Item it; it.kind = Item::TAU; it.n = n_children;
        cur.push_back(it);
        return;
    }

    if (L == "gamma") {
        Node* rator = n->child;
        Node* rand  = rator->sibling;
        flattenInto(rator, cur);
        flattenInto(rand,  cur);
        Item it; it.kind = Item::GAMMA;
        cur.push_back(it);
        return;
    }

    // Binary operators
    static const std::vector<std::string> binops = {
        "+", "-", "*", "/", "**", "or", "&",
        "gr", "ge", "ls", "le", "eq", "ne", "aug"
    };
    for (auto& bop : binops) {
        if (L == bop) {
            Node* lhs = n->child;
            Node* rhs = lhs->sibling;
            flattenInto(lhs, cur);
            flattenInto(rhs, cur);
            Item it; it.kind = Item::OP; it.sval = L;
            cur.push_back(it);
            return;
        }
    }

    // Unary
    if (L == "neg" || L == "not") {
        flattenInto(n->child, cur);
        Item it; it.kind = Item::OP; it.sval = L;
        cur.push_back(it);
        return;
    }

    throw std::runtime_error("CSE: unknown ST label '" + L + "'");
}

int CSEMachine::flatten(Node* n) {
    int k = (int)deltas.size();
    deltas.emplace_back();
    std::vector<Item> d;
    flattenInto(n, d);
    deltas[k] = std::move(d);
    return k;
}

// ============================================================
// Primitive bindings (initial environment)
// ============================================================
void CSEMachine::initPrimitives() {
    EnvPtr e0 = new Environment(0, nullptr);
    allEnvs.push_back(e0);
    nextEnvIdx = 1;

    auto addBuiltin = [&](const std::string& name) {
        Value v; v.kind = Value::BUILTIN; v.builtinName = name;
        e0->bindings[name] = v;
    };
    addBuiltin("Print");
    addBuiltin("print");
    addBuiltin("Stem");
    addBuiltin("Stern");
    addBuiltin("Conc");
    addBuiltin("Order");
    addBuiltin("Null");
    addBuiltin("Isinteger");
    addBuiltin("Isstring");
    addBuiltin("Istuple");
    addBuiltin("Isfunction");
    addBuiltin("Isdummy");
    addBuiltin("Istruthvalue");
    addBuiltin("ItoS");

    currentEnv = e0;
}

// ============================================================
// Bind argument to a lambda's parameter spec
// ============================================================
void CSEMachine::bindArg(EnvPtr env, const LambdaVar& var, const Value& arg) {
    switch (var.kind) {
        case LambdaVar::SINGLE:
            env->bindings[var.names[0]] = arg;
            break;
        case LambdaVar::TUPLE:
            if (arg.kind != Value::TUPLE) {
                throw std::runtime_error("Expected tuple argument for multi-parameter lambda");
            }
            if (arg.tupleVals.size() != var.names.size()) {
                throw std::runtime_error("Tuple arity mismatch in lambda application");
            }
            for (size_t i = 0; i < var.names.size(); i++) {
                env->bindings[var.names[i]] = arg.tupleVals[i];
            }
            break;
        case LambdaVar::EMPTY:
            // no bindings
            break;
    }
}

// ============================================================
// Push items of delta_k onto control (reversed so delta_k[0] is on top)
// ============================================================
void CSEMachine::pushDelta(int k) {
    auto& d = deltas[k];
    for (int i = (int)d.size() - 1; i >= 0; --i) {
        control.push_back(d[i]);
    }
}

// ============================================================
// Operators
// ============================================================
static Value makeInt(long long n)  { Value v; v.kind = Value::INT;  v.intVal = n; return v; }
static Value makeBool(bool b)      { Value v; v.kind = Value::BOOL; v.boolVal = b; return v; }
static Value makeStr(const std::string& s) { Value v; v.kind = Value::STR; v.strVal = s; return v; }
static Value makeNil()             { Value v; v.kind = Value::NIL; return v; }
static Value makeDummy()           { Value v; v.kind = Value::DUMMY; return v; }

static bool valEquals(const Value& a, const Value& b) {
    if (a.kind != b.kind) {
        // BOOL/INT not equal across types
        return false;
    }
    switch (a.kind) {
        case Value::INT:  return a.intVal == b.intVal;
        case Value::STR:  return a.strVal == b.strVal;
        case Value::BOOL: return a.boolVal == b.boolVal;
        case Value::NIL:  return true;
        case Value::DUMMY: return true;
        default: return false;
    }
}

void CSEMachine::doOp(const std::string& op) {
    if (op == "neg") {
        Value a = stack.back(); stack.pop_back();
        if (a.kind != Value::INT) throw std::runtime_error("neg requires integer");
        stack.push_back(makeInt(-a.intVal));
        return;
    }
    if (op == "not") {
        Value a = stack.back(); stack.pop_back();
        if (a.kind != Value::BOOL) throw std::runtime_error("not requires boolean");
        stack.push_back(makeBool(!a.boolVal));
        return;
    }
    // Binary: top = rhs, next = lhs
    Value rhs = stack.back(); stack.pop_back();
    Value lhs = stack.back(); stack.pop_back();

    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "**") {
        if (lhs.kind != Value::INT || rhs.kind != Value::INT)
            throw std::runtime_error("Arithmetic on non-integer");
        long long a = lhs.intVal, b = rhs.intVal, r = 0;
        if (op == "+") r = a + b;
        else if (op == "-") r = a - b;
        else if (op == "*") r = a * b;
        else if (op == "/") {
            if (b == 0) throw std::runtime_error("Division by zero");
            r = a / b;
        } else { // **
            r = 1;
            long long base = a;
            long long exp = b;
            if (exp < 0) throw std::runtime_error("Negative exponent");
            while (exp > 0) {
                if (exp & 1) r *= base;
                base *= base;
                exp >>= 1;
            }
        }
        stack.push_back(makeInt(r));
        return;
    }
    if (op == "gr" || op == "ge" || op == "ls" || op == "le") {
        if (lhs.kind != Value::INT || rhs.kind != Value::INT)
            throw std::runtime_error("Comparison on non-integer");
        long long a = lhs.intVal, b = rhs.intVal;
        bool r = false;
        if (op == "gr") r = a > b;
        else if (op == "ge") r = a >= b;
        else if (op == "ls") r = a < b;
        else r = a <= b;
        stack.push_back(makeBool(r));
        return;
    }
    if (op == "eq") { stack.push_back(makeBool(valEquals(lhs, rhs))); return; }
    if (op == "ne") { stack.push_back(makeBool(!valEquals(lhs, rhs))); return; }
    if (op == "or") {
        if (lhs.kind != Value::BOOL || rhs.kind != Value::BOOL)
            throw std::runtime_error("'or' requires booleans");
        stack.push_back(makeBool(lhs.boolVal || rhs.boolVal));
        return;
    }
    if (op == "&") {
        if (lhs.kind != Value::BOOL || rhs.kind != Value::BOOL)
            throw std::runtime_error("'&' requires booleans");
        stack.push_back(makeBool(lhs.boolVal && rhs.boolVal));
        return;
    }
    if (op == "aug") {
        // augment a tuple with an element
        Value t;
        t.kind = Value::TUPLE;
        if (lhs.kind == Value::NIL) {
            t.tupleVals.push_back(rhs);
        } else if (lhs.kind == Value::TUPLE) {
            t.tupleVals = lhs.tupleVals;
            t.tupleVals.push_back(rhs);
        } else {
            throw std::runtime_error("aug requires tuple/nil as left operand");
        }
        stack.push_back(t);
        return;
    }
    throw std::runtime_error("Unknown operator: " + op);
}

// ============================================================
// Built-in application
// ============================================================
void CSEMachine::applyBuiltin(Value rator, Value rand) {
    const std::string& name = rator.builtinName;

    if (name == "Print" || name == "print") {
        (*out_) << formatValue(rand);
        stack.push_back(makeDummy());
        return;
    }
    if (name == "Order") {
        if (rand.kind == Value::NIL) { stack.push_back(makeInt(0)); return; }
        if (rand.kind == Value::TUPLE) { stack.push_back(makeInt((long long)rand.tupleVals.size())); return; }
        throw std::runtime_error("Order requires tuple");
    }
    if (name == "Null") {
        bool b = (rand.kind == Value::NIL) ||
                 (rand.kind == Value::TUPLE && rand.tupleVals.empty());
        stack.push_back(makeBool(b));
        return;
    }
    if (name == "Isinteger") { stack.push_back(makeBool(rand.kind == Value::INT)); return; }
    if (name == "Isstring")  { stack.push_back(makeBool(rand.kind == Value::STR)); return; }
    if (name == "Istuple")   { stack.push_back(makeBool(rand.kind == Value::TUPLE || rand.kind == Value::NIL)); return; }
    if (name == "Isfunction"){
        bool b = (rand.kind == Value::CLOSURE || rand.kind == Value::ETA_CLOSURE ||
                  rand.kind == Value::BUILTIN || rand.kind == Value::PARTIAL_BUILTIN);
        stack.push_back(makeBool(b)); return;
    }
    if (name == "Isdummy")        { stack.push_back(makeBool(rand.kind == Value::DUMMY)); return; }
    if (name == "Istruthvalue")   { stack.push_back(makeBool(rand.kind == Value::BOOL)); return; }
    if (name == "Stem") {
        if (rand.kind != Value::STR) throw std::runtime_error("Stem requires string");
        if (rand.strVal.empty()) { stack.push_back(makeStr("")); return; }
        stack.push_back(makeStr(rand.strVal.substr(0, 1))); return;
    }
    if (name == "Stern") {
        if (rand.kind != Value::STR) throw std::runtime_error("Stern requires string");
        if (rand.strVal.empty()) { stack.push_back(makeStr("")); return; }
        stack.push_back(makeStr(rand.strVal.substr(1))); return;
    }
    if (name == "ItoS") {
        if (rand.kind != Value::INT) throw std::runtime_error("ItoS requires integer");
        stack.push_back(makeStr(std::to_string(rand.intVal))); return;
    }
    if (name == "Conc") {
        // curried: first call saves the arg; second call concatenates
        if (rator.kind == Value::BUILTIN) {
            Value partial; partial.kind = Value::PARTIAL_BUILTIN;
            partial.builtinName = "Conc";
            partial.partialArgs.push_back(rand);
            stack.push_back(partial);
            return;
        } else {
            // PARTIAL_BUILTIN
            Value first = rator.partialArgs[0];
            if (first.kind != Value::STR || rand.kind != Value::STR)
                throw std::runtime_error("Conc requires strings");
            stack.push_back(makeStr(first.strVal + rand.strVal));
            return;
        }
    }
    throw std::runtime_error("Unknown built-in: " + name);
}

// ============================================================
// Gamma application
// ============================================================
void CSEMachine::doGamma() {
    // Flatten emits rator items, then rand items, then Gamma.
    // So stack top = rand_value, below = rator_value.
    Value rand  = stack.back(); stack.pop_back();
    Value rator = stack.back(); stack.pop_back();

    if (rator.kind == Value::CLOSURE) {
        // Apply lambda
        EnvPtr newEnv = new Environment(nextEnvIdx++, rator.closureEnv);
        allEnvs.push_back(newEnv);
        bindArg(newEnv, rator.closureVar, rand);

        // Push env marker on control to restore env after body
        Item em; em.kind = Item::ENV_MARKER; em.envPtr = currentEnv;
        control.push_back(em);

        // Push body delta items (reversed) so first body item is on top
        pushDelta(rator.closureK);

        currentEnv = newEnv;
        return;
    }
    if (rator.kind == Value::BUILTIN || rator.kind == Value::PARTIAL_BUILTIN) {
        applyBuiltin(rator, rand);
        return;
    }
    if (rator.kind == Value::YSTAR) {
        // Y* applied to lambda closure -> eta-closure
        if (rand.kind != Value::CLOSURE)
            throw std::runtime_error("Y* expects a function");
        Value eta = rand;
        eta.kind = Value::ETA_CLOSURE;
        stack.push_back(eta);
        return;
    }
    if (rator.kind == Value::ETA_CLOSURE) {
        // eta applied to v: rewrite as (L eta) v, where L is the same lambda as a regular closure.
        // Execution order desired: push L, push eta, gamma1, push v, gamma2.
        // Stack to start: [..., L, eta] (top = eta). Then gamma1 -> [..., C].
        // Then push v -> [..., C, v]. Then gamma2 -> [..., result].
        Value f = rator;
        f.kind = Value::CLOSURE;

        stack.push_back(f);          // L (deeper)
        stack.push_back(rator);      // eta on top

        Item g; g.kind = Item::GAMMA;
        Item pv; pv.kind = Item::LITERAL_VAL; pv.litVal = rand;
        // control push order: last pushed = first executed
        control.push_back(g);        // gamma2 (executes last)
        control.push_back(pv);       // push v (executes second)
        control.push_back(g);        // gamma1 (executes first)
        return;
    }
    if (rator.kind == Value::TUPLE) {
        // tuple indexing: rand should be integer
        if (rand.kind != Value::INT)
            throw std::runtime_error("Tuple indexing requires integer");
        long long idx = rand.intVal;
        if (idx < 1 || idx > (long long)rator.tupleVals.size())
            throw std::runtime_error("Tuple index out of range");
        stack.push_back(rator.tupleVals[(size_t)idx - 1]);
        return;
    }
    if (rator.kind == Value::STR) {
        // (rarely used) string indexing
        if (rand.kind != Value::INT)
            throw std::runtime_error("String indexing requires integer");
        long long idx = rand.intVal;
        if (idx < 1 || idx > (long long)rator.strVal.size())
            throw std::runtime_error("String index out of range");
        stack.push_back(makeStr(rator.strVal.substr((size_t)idx - 1, 1)));
        return;
    }

    throw std::runtime_error("Gamma: invalid rator kind");
}

// ============================================================
// Format a value for Print
// ============================================================
std::string CSEMachine::formatValue(const Value& v) {
    std::ostringstream o;
    switch (v.kind) {
        case Value::INT:   o << v.intVal; break;
        case Value::STR:   o << v.strVal; break;
        case Value::BOOL:  o << (v.boolVal ? "true" : "false"); break;
        case Value::NIL:   o << "nil"; break;
        case Value::DUMMY: o << "dummy"; break;
        case Value::TUPLE:
            if (v.tupleVals.empty()) { o << "nil"; break; }
            o << "(";
            for (size_t i = 0; i < v.tupleVals.size(); i++) {
                if (i) o << ", ";
                o << formatValue(v.tupleVals[i]);
            }
            o << ")";
            break;
        case Value::CLOSURE:
        case Value::ETA_CLOSURE:
            o << "[lambda closure: ";
            if (v.closureVar.kind == LambdaVar::SINGLE)
                o << v.closureVar.names[0];
            else if (v.closureVar.kind == LambdaVar::TUPLE) {
                o << "(";
                for (size_t i = 0; i < v.closureVar.names.size(); i++) {
                    if (i) o << ",";
                    o << v.closureVar.names[i];
                }
                o << ")";
            } else {
                o << "()";
            }
            o << ": " << v.closureK << "]";
            break;
        case Value::BUILTIN:
        case Value::PARTIAL_BUILTIN:
            o << v.builtinName;
            break;
        default:
            o << "?";
            break;
    }
    return o.str();
}

// ============================================================
// Main run loop
// ============================================================
void CSEMachine::run(std::ostream& out) {
    out_ = &out;

    initPrimitives();
    int k0 = flatten(root);     // delta_0

    // Push env marker for env 0, then push delta_0 items (reversed)
    Item em0; em0.kind = Item::ENV_MARKER; em0.envPtr = nullptr;
    control.push_back(em0);
    pushDelta(k0);

    while (!control.empty()) {
        Item it = control.back(); control.pop_back();

        switch (it.kind) {
            case Item::NAME: {
                if (!currentEnv->has(it.sval))
                    throw std::runtime_error("Undefined name: " + it.sval);
                stack.push_back(currentEnv->lookup(it.sval));
                break;
            }
            case Item::INT_LIT:   stack.push_back(makeInt(it.ival)); break;
            case Item::STR_LIT:   stack.push_back(makeStr(it.sval)); break;
            case Item::TRUE_LIT:  stack.push_back(makeBool(true)); break;
            case Item::FALSE_LIT: stack.push_back(makeBool(false)); break;
            case Item::NIL_LIT:   stack.push_back(makeNil()); break;
            case Item::DUMMY_LIT: stack.push_back(makeDummy()); break;
            case Item::YSTAR_LIT: {
                Value v; v.kind = Value::YSTAR; stack.push_back(v); break;
            }
            case Item::LAMBDA: {
                Value v; v.kind = Value::CLOSURE;
                v.closureK = it.deltaIdx;
                v.closureVar = it.var;
                v.closureEnv = currentEnv;
                stack.push_back(v);
                break;
            }
            case Item::GAMMA: doGamma(); break;
            case Item::TAU: {
                Value t; t.kind = Value::TUPLE;
                t.tupleVals.resize(it.n);
                for (int i = it.n - 1; i >= 0; --i) {
                    t.tupleVals[i] = stack.back();
                    stack.pop_back();
                }
                stack.push_back(t);
                break;
            }
            case Item::OP: doOp(it.sval); break;
            case Item::BETA: {
                Value cond = stack.back(); stack.pop_back();
                if (cond.kind != Value::BOOL)
                    throw std::runtime_error("Conditional requires boolean");
                Item drThen = control.back(); control.pop_back();
                Item drElse = control.back(); control.pop_back();
                int k = cond.boolVal ? drThen.deltaIdx : drElse.deltaIdx;
                pushDelta(k);
                break;
            }
            case Item::DELTA_REF:
                // should be consumed only by Beta
                throw std::runtime_error("Unexpected DELTA_REF in control");
            case Item::ENV_MARKER:
                // restore previous env
                if (it.envPtr) currentEnv = it.envPtr;
                break;
            case Item::LITERAL_VAL:
                stack.push_back(it.litVal);
                break;
        }
    }
}
