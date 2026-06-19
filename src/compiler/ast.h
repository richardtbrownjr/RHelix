// ast.h - Abstract Syntax Tree definitions for RHelix
#ifndef AST_H
#define AST_H

#include "token.h"

typedef struct ASTNode ASTNode;

typedef enum {
    // Expressions
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_LITERAL_STRING,
    AST_LITERAL_BOOL,
    AST_LITERAL_NONE,
    AST_IDENTIFIER,
    AST_BINARY,
    AST_UNARY,
    AST_GROUPING,
    // Statements
    AST_EXPRESSION_STMT,
    AST_ASSIGNMENT,
    AST_RETURN,
    // Top level
    AST_MODULE
} ASTNodeType;

// === Expression payloads ===

typedef struct { long value; } ASTLiteralInt;
typedef struct { double value; } ASTLiteralFloat;
typedef struct { char* value; } ASTLiteralString;
typedef struct { int value; } ASTLiteralBool;
typedef struct { char* name; } ASTIdentifier;

typedef struct {
    TokenType op;
    ASTNode* left;
    ASTNode* right;
} ASTBinary;

typedef struct {
    TokenType op;
    ASTNode* operand;
} ASTUnary;

typedef struct {
    ASTNode* expression;
} ASTGrouping;

// === Statement payloads ===

typedef struct {
    ASTNode* expression;
} ASTExpressionStmt;

typedef struct {
    ASTNode* target;       // For v1: must be AST_IDENTIFIER
    ASTNode* value;
} ASTAssignment;

typedef struct {
    ASTNode* value;        // Optional; NULL for bare "return"
} ASTReturn;

// === Module (top level) ===

typedef struct {
    ASTNode** statements;  // Dynamic array of owned statement nodes
    int count;
    int capacity;
} ASTModule;

// === The tagged union ===

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
        ASTExpressionStmt expression_stmt;
        ASTAssignment assignment;
        ASTReturn ret;     // 'return' is a C keyword; member is 'ret'
        ASTModule module;
    } as;
};

// === Expression constructors ===

ASTNode* ast_literal_int(long value, int line, int column);
ASTNode* ast_literal_float(double value, int line, int column);
ASTNode* ast_literal_string(const char* value, int line, int column);
ASTNode* ast_literal_bool(int value, int line, int column);
ASTNode* ast_literal_none(int line, int column);
ASTNode* ast_identifier(const char* name, int line, int column);
ASTNode* ast_binary(TokenType op, ASTNode* left, ASTNode* right, int line, int column);
ASTNode* ast_unary(TokenType op, ASTNode* operand, int line, int column);
ASTNode* ast_grouping(ASTNode* expression, int line, int column);

// === Statement constructors ===

ASTNode* ast_expression_stmt(ASTNode* expression, int line, int column);
ASTNode* ast_assignment(ASTNode* target, ASTNode* value, int line, int column);
ASTNode* ast_return(ASTNode* value, int line, int column);  // value may be NULL

// === Module ===

ASTNode* ast_module(int line, int column);
void ast_module_add(ASTNode* module, ASTNode* statement);

// === Lifecycle and debug ===

void ast_destroy(ASTNode* node);
void ast_print(ASTNode* node, int indent);
const char* ast_node_type_to_string(ASTNodeType type);

#endif // AST_H
