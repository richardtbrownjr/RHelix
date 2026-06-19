// ast.c - Abstract Syntax Tree implementation for RHelix
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal helper: allocate a node and set the common fields.
// Caller is responsible for setting node->as.<member> before returning.
static ASTNode* make_node(ASTNodeType type, int line, int column) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    node->line = line;
    node->column = column;
    return node;
}

// ===== Constructors =====

ASTNode* ast_literal_int(long value, int line, int column) {
    ASTNode* node = make_node(AST_LITERAL_INT, line, column);
    if (!node) return NULL;
    node->as.literal_int.value = value;
    return node;
}

ASTNode* ast_literal_float(double value, int line, int column) {
    ASTNode* node = make_node(AST_LITERAL_FLOAT, line, column);
    if (!node) return NULL;
    node->as.literal_float.value = value;
    return node;
}

ASTNode* ast_literal_string(const char* value, int line, int column) {
    ASTNode* node = make_node(AST_LITERAL_STRING, line, column);
    if (!node) return NULL;
    node->as.literal_string.value = value ? strdup(value) : NULL;
    return node;
}

ASTNode* ast_literal_bool(int value, int line, int column) {
    ASTNode* node = make_node(AST_LITERAL_BOOL, line, column);
    if (!node) return NULL;
    node->as.literal_bool.value = value ? 1 : 0;
    return node;
}

ASTNode* ast_literal_none(int line, int column) {
    return make_node(AST_LITERAL_NONE, line, column);
}

ASTNode* ast_identifier(const char* name, int line, int column) {
    ASTNode* node = make_node(AST_IDENTIFIER, line, column);
    if (!node) return NULL;
    node->as.identifier.name = name ? strdup(name) : NULL;
    return node;
}

ASTNode* ast_binary(TokenType op, ASTNode* left, ASTNode* right, int line, int column) {
    ASTNode* node = make_node(AST_BINARY, line, column);
    if (!node) return NULL;
    node->as.binary.op = op;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

ASTNode* ast_unary(TokenType op, ASTNode* operand, int line, int column) {
    ASTNode* node = make_node(AST_UNARY, line, column);
    if (!node) return NULL;
    node->as.unary.op = op;
    node->as.unary.operand = operand;
    return node;
}

ASTNode* ast_grouping(ASTNode* expression, int line, int column) {
    ASTNode* node = make_node(AST_GROUPING, line, column);
    if (!node) return NULL;
    node->as.grouping.expression = expression;
    return node;
}

// ===== Destructor =====

void ast_destroy(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
            // No heap-allocated members
            break;
        case AST_LITERAL_STRING:
            free(node->as.literal_string.value);
            break;
        case AST_IDENTIFIER:
            free(node->as.identifier.name);
            break;
        case AST_BINARY:
            ast_destroy(node->as.binary.left);
            ast_destroy(node->as.binary.right);
            break;
        case AST_UNARY:
            ast_destroy(node->as.unary.operand);
            break;
        case AST_GROUPING:
            ast_destroy(node->as.grouping.expression);
            break;
    }

    free(node);
}

// ===== Debug helpers =====

const char* ast_node_type_to_string(ASTNodeType type) {
    switch (type) {
        case AST_LITERAL_INT: return "LiteralInt";
        case AST_LITERAL_FLOAT: return "LiteralFloat";
        case AST_LITERAL_STRING: return "LiteralString";
        case AST_LITERAL_BOOL: return "LiteralBool";
        case AST_LITERAL_NONE: return "LiteralNone";
        case AST_IDENTIFIER: return "Identifier";
        case AST_BINARY: return "Binary";
        case AST_UNARY: return "Unary";
        case AST_GROUPING: return "Grouping";
        default: return "UNKNOWN";
    }
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print(ASTNode* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    print_indent(indent);

    switch (node->type) {
        case AST_LITERAL_INT:
            printf("LiteralInt(%ld)\n", node->as.literal_int.value);
            break;
        case AST_LITERAL_FLOAT:
            printf("LiteralFloat(%g)\n", node->as.literal_float.value);
            break;
        case AST_LITERAL_STRING:
            printf("LiteralString(\"%s\")\n",
                   node->as.literal_string.value ? node->as.literal_string.value : "");
            break;
        case AST_LITERAL_BOOL:
            printf("LiteralBool(%s)\n",
                   node->as.literal_bool.value ? "True" : "False");
            break;
        case AST_LITERAL_NONE:
            printf("LiteralNone\n");
            break;
        case AST_IDENTIFIER:
            printf("Identifier(%s)\n",
                   node->as.identifier.name ? node->as.identifier.name : "");
            break;
        case AST_BINARY:
            printf("Binary(%s)\n", token_type_to_string(node->as.binary.op));
            ast_print(node->as.binary.left, indent + 1);
            ast_print(node->as.binary.right, indent + 1);
            break;
        case AST_UNARY:
            printf("Unary(%s)\n", token_type_to_string(node->as.unary.op));
            ast_print(node->as.unary.operand, indent + 1);
            break;
        case AST_GROUPING:
            printf("Grouping\n");
            ast_print(node->as.grouping.expression, indent + 1);
            break;
    }
}
