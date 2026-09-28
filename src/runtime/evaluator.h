// evaluator.h - Tree-walking interpreter for RHelix
//
// The evaluator takes an expression AST and computes its runtime Value.
// This is the first "code actually runs" milestone: literals produce
// Values, arithmetic actually computes, comparisons return real bools,
// logical operators short-circuit correctly.
//
// Session 2 scope: expressions only. Statements, variables, function
// calls, collections, and class instances are deferred to later sessions.
//
// Ownership contract:
// - evaluate returns a Value by value. The caller owns any heap data
//   the Value contains (e.g., VAL_STRING's char buffer) and MUST call
//   value_destroy when done.
// - During recursive evaluation, any Value produced but not returned
//   must be destroyed to avoid leaks (e.g., left/right operands of a
//   binary operation after the result is computed).
//
// Runtime errors: the static analyzer catches most type mismatches at
// parse time. When the evaluator encounters an operation it cannot
// perform, it prints an error to stderr and returns VAL_NONE.

#ifndef RHELIX_EVALUATOR_H
#define RHELIX_EVALUATOR_H

#include "value.h"
#include "environment.h"
#include "../compiler/ast.h"


// Evaluate an expression AST node and return the computed Value.
// Never returns NULL - errors produce VAL_NONE with an error printed
// to stderr.
Value evaluate(ASTNode* node, Environment* env);

// Evaluate a statement AST node. Executes for side effects.
// Returns the wrapped expression's Value for AST_EXPRESSION_STMT
// (so REPL callers can print it), the last statement's value for
// AST_MODULE and AST_BLOCK (Python REPL semantic), VAL_NONE otherwise.
// Result of executing a statement. If `returned` is true, a return
// statement fired somewhere in the executed subtree and the value
// should be propagated upward (the enclosing function call site
// consumes it, or the REPL treats it as the module's value).
// Signal that a statement produced. Most statements produce SIGNAL_NORMAL.
// return, break, and continue produce their respective signals which
// propagate up through nested blocks until consumed by the enclosing
// function call (SIGNAL_RETURN) or loop (SIGNAL_BREAK/SIGNAL_CONTINUE).
typedef enum {
    SIGNAL_NORMAL,
    SIGNAL_RETURN,
    SIGNAL_BREAK,
    SIGNAL_CONTINUE,
} StmtSignal;

// Result of executing a statement. The signal field tells enclosing
// scopes how to react; the value carries the return value (for
// SIGNAL_RETURN) or the last-evaluated Value (for SIGNAL_NORMAL).
typedef struct StmtResult {
    Value value;
    StmtSignal signal;
} StmtResult;

// Evaluate a statement AST node. Executes for side effects.
// Returns StmtResult { value, signal }. Signal tells enclosing scope
// how to react (return, break, continue, or nothing).
StmtResult evaluate_statement(ASTNode* node, Environment* env);


#endif // RHELIX_EVALUATOR_H
