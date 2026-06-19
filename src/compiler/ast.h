// ast.h - Abstract Syntax Tree definitions for RHelix
#ifndef AST_H
#define AST_H

#include "token.h"

// Forward declaration so nodes can reference each other
typedef struct ASTNode ASTNode;

// Every node has one of these types. The tag tells you which member of
// the union below is valid.
typedef enum {
    // Expressions
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_LITERAL_STRING,
    AST_LITERAL_BOOL,
    AST_LITERAL_NONE,
    AST_IDENTIFIER,
    AST_BINARY,        // left op right     (e.g., a + b)
    AST_UNARY,         // op operand        (e.g., -x)
    AST_GROUPING       // (expression)
} ASTNodeType;

// Node-specific data lives in these structs, accessed via the union below

typedef struct {
    long value;
} ASTLiteralInt;

typedef struct {
    double value;
} ASTLiteralFloat;

typedef struct {
    char* value;        // Owned by the node, freed on destroy
} ASTLiteralString;

typedef struct {
    int value;          // 1 for True, 0 for False
} ASTLiteralBool;

typedef struct {
    char* name;         // Owned by the node
} ASTIdentifier;

typedef struct {
    TokenType op;       // The operator token type (e.g., TOKEN_PLUS)
    ASTNode* left;      // Owned
    ASTNode* right;     // Owned
} ASTBinary;

typedef struct {
    TokenType op;       // The operator token type (e.g., TOKEN_MINUS)
    ASTNode* operand;   // Owned
} ASTUnary;

typedef struct {
    ASTNode* expression;  // Owned
} ASTGrouping;

// The actual node. Type tag plus union of all possible node payloads.
// Line and column come from the originating token for error reporting.
struct ASTNode {
    ASTNodeType type;
    int line;
    int column;
    union {
        ASTLiteralInt literal_int;
        ASTLiteralFloat literal_float;
        ASTLiteralString literal_string;
        ASTLiteralBool literal_bool;
        ASTIdentifier identifier;
        ASTBinary binary;
        ASTUnary unary;
        ASTGrouping grouping;
        // AST_LITERAL_NONE has no data
    } as;
};

// Constructor functions. Each takes the data and produces a heap-allocated
// node. The caller owns the returned node and must destroy it.
ASTNode* ast_literal_int(long value, int line, int column);
ASTNode* ast_literal_float(double value, int line, int column);
ASTNode* ast_literal_string(const char* value, int line, int column);
ASTNode* ast_literal_bool(int value, int line, int column);
ASTNode* ast_literal_none(int line, int column);
ASTNode* ast_identifier(const char* name, int line, int column);
ASTNode* ast_binary(TokenType op, ASTNode* left, ASTNode* right, int line, int column);
ASTNode* ast_unary(TokenType op, ASTNode* operand, int line, int column);
ASTNode* ast_grouping(ASTNode* expression, int line, int column);

// Recursively destroy a node and all its children.
void ast_destroy(ASTNode* node);

// Pretty-print an AST to stdout. Indent should start at 0.
void ast_print(ASTNode* node, int indent);

// Convert a node type enum to a printable name (for debugging).
const char* ast_node_type_to_string(ASTNodeType type);

#endif // AST_H
