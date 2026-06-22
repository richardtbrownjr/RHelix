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
    AST_CALL,
    AST_SUBSCRIPT,
    AST_ATTRIBUTE,
    // Simple statements
    AST_EXPRESSION_STMT,
    AST_ASSIGNMENT,
    AST_RETURN,
    AST_PASS,
    AST_BREAK,
    AST_CONTINUE,
    // Compound statements
    AST_BLOCK,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    // Declarations
    AST_FUNCTION_DEF,
    AST_CLASS_DEF,
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

typedef struct {
    ASTNode* callee;
    ASTNode** args;
    int arg_count;
    int arg_capacity;
} ASTCall;

typedef struct {
    ASTNode* object;
    ASTNode* index;
} ASTSubscript;

typedef struct {
    ASTNode* object;
    char* name;
} ASTAttribute;

// === Simple statement payloads ===

typedef struct {
    ASTNode* expression;
} ASTExpressionStmt;

typedef struct {
    ASTNode* target;
    ASTNode* value;
} ASTAssignment;

typedef struct {
    ASTNode* value;
} ASTReturn;

// AST_PASS has no payload - it is a tag-only node

// === Compound statement payloads ===

typedef struct {
    ASTNode** statements;
    int count;
    int capacity;
} ASTBlock;

typedef struct {
    ASTNode* condition;
    ASTNode* then_block;
    ASTNode* else_block;
} ASTIf;

typedef struct {
    ASTNode* condition;
    ASTNode* body;
} ASTWhile;

typedef struct {
    char* var_name;
    ASTNode* iterable;
    ASTNode* body;
} ASTFor;

// === Declaration payloads ===

typedef struct {
    char* name;
    ASTNode* type_annotation;
} ASTParam;

typedef struct {
    char* name;
    ASTParam* params;
    int param_count;
    int param_capacity;
    ASTNode* return_type;
    ASTNode* body;
    ASTNode** decorators;        // Dynamic array of decorator expressions
    int decorator_count;
    int decorator_capacity;
} ASTFunctionDef;

typedef struct {
    char* name;
    ASTNode** base_classes;      // Dynamic array of identifier nodes
    int base_count;
    int base_capacity;
    ASTNode* body;               // AST_BLOCK
    ASTNode** decorators;        // Dynamic array of decorator expressions
    int decorator_count;
    int decorator_capacity;
} ASTClassDef;

// === Module ===

typedef struct {
    ASTNode** statements;
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
        ASTCall call;
        ASTSubscript subscript;
        ASTAttribute attribute;
        ASTExpressionStmt expression_stmt;
        ASTAssignment assignment;
        ASTReturn ret;
        ASTBlock block;
        ASTIf if_stmt;
        ASTWhile while_stmt;
        ASTFor for_stmt;
        ASTFunctionDef function_def;
        ASTClassDef class_def;
        ASTModule module;
        // AST_PASS, AST_LITERAL_NONE have no payload
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
ASTNode* ast_call(ASTNode* callee, int line, int column);
void ast_call_add_arg(ASTNode* call, ASTNode* arg);
ASTNode* ast_subscript(ASTNode* object, ASTNode* index, int line, int column);
ASTNode* ast_attribute(ASTNode* object, const char* name, int line, int column);

// === Simple statement constructors ===

ASTNode* ast_expression_stmt(ASTNode* expression, int line, int column);
ASTNode* ast_assignment(ASTNode* target, ASTNode* value, int line, int column);
ASTNode* ast_return(ASTNode* value, int line, int column);
ASTNode* ast_pass(int line, int column);
ASTNode* ast_break(int line, int column);
ASTNode* ast_continue(int line, int column);

// === Compound statement constructors ===

ASTNode* ast_block(int line, int column);
void ast_block_add(ASTNode* block, ASTNode* statement);
ASTNode* ast_if(ASTNode* condition, ASTNode* then_block, ASTNode* else_block,
                int line, int column);
ASTNode* ast_while(ASTNode* condition, ASTNode* body, int line, int column);
ASTNode* ast_for(const char* var_name, ASTNode* iterable, ASTNode* body,
                 int line, int column);

// === Declaration constructors ===

ASTNode* ast_function_def(const char* name, ASTNode* return_type,
                          ASTNode* body, int line, int column);
void ast_function_def_add_param(ASTNode* func_def, const char* param_name,
                                ASTNode* type_annotation);
ASTNode* ast_class_def(const char* name, ASTNode* body, int line, int column);
void ast_class_def_add_base(ASTNode* class_def, const char* base_name,
                            int line, int column);
void ast_function_def_add_decorator(ASTNode* func_def, ASTNode* decorator);
void ast_class_def_add_decorator(ASTNode* class_def, ASTNode* decorator);

// === Module ===

ASTNode* ast_module(int line, int column);
void ast_module_add(ASTNode* module, ASTNode* statement);

// === Lifecycle and debug ===

void ast_destroy(ASTNode* node);
void ast_print(ASTNode* node, int indent);
const char* ast_node_type_to_string(ASTNodeType type);

#endif // AST_H
