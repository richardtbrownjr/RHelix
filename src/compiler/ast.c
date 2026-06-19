// ast.c - Abstract Syntax Tree implementation for RHelix
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ASTNode* make_node(ASTNodeType type, int line, int column) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    node->line = line;
    node->column = column;
    return node;
}

// ===== Expression constructors =====

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

ASTNode* ast_call(ASTNode* callee, int line, int column) {
    ASTNode* node = make_node(AST_CALL, line, column);
    if (!node) return NULL;
    node->as.call.callee = callee;
    node->as.call.args = NULL;
    node->as.call.arg_count = 0;
    node->as.call.arg_capacity = 0;
    return node;
}

void ast_call_add_arg(ASTNode* call, ASTNode* arg) {
    if (!call || call->type != AST_CALL || !arg) return;
    ASTCall* c = &call->as.call;
    if (c->arg_count >= c->arg_capacity) {
        int new_cap = c->arg_capacity == 0 ? 4 : c->arg_capacity * 2;
        c->args = (ASTNode**)realloc(c->args, sizeof(ASTNode*) * new_cap);
        c->arg_capacity = new_cap;
    }
    c->args[c->arg_count++] = arg;
}

// ===== Simple statement constructors =====

ASTNode* ast_expression_stmt(ASTNode* expression, int line, int column) {
    ASTNode* node = make_node(AST_EXPRESSION_STMT, line, column);
    if (!node) return NULL;
    node->as.expression_stmt.expression = expression;
    return node;
}

ASTNode* ast_assignment(ASTNode* target, ASTNode* value, int line, int column) {
    ASTNode* node = make_node(AST_ASSIGNMENT, line, column);
    if (!node) return NULL;
    node->as.assignment.target = target;
    node->as.assignment.value = value;
    return node;
}

ASTNode* ast_return(ASTNode* value, int line, int column) {
    ASTNode* node = make_node(AST_RETURN, line, column);
    if (!node) return NULL;
    node->as.ret.value = value;
    return node;
}

// ===== Compound statement constructors =====

ASTNode* ast_block(int line, int column) {
    ASTNode* node = make_node(AST_BLOCK, line, column);
    if (!node) return NULL;
    node->as.block.statements = NULL;
    node->as.block.count = 0;
    node->as.block.capacity = 0;
    return node;
}

void ast_block_add(ASTNode* block, ASTNode* statement) {
    if (!block || block->type != AST_BLOCK || !statement) return;
    ASTBlock* b = &block->as.block;
    if (b->count >= b->capacity) {
        int new_cap = b->capacity == 0 ? 8 : b->capacity * 2;
        b->statements = (ASTNode**)realloc(b->statements, sizeof(ASTNode*) * new_cap);
        b->capacity = new_cap;
    }
    b->statements[b->count++] = statement;
}

ASTNode* ast_if(ASTNode* condition, ASTNode* then_block, ASTNode* else_block,
                int line, int column) {
    ASTNode* node = make_node(AST_IF, line, column);
    if (!node) return NULL;
    node->as.if_stmt.condition = condition;
    node->as.if_stmt.then_block = then_block;
    node->as.if_stmt.else_block = else_block;
    return node;
}

ASTNode* ast_while(ASTNode* condition, ASTNode* body, int line, int column) {
    ASTNode* node = make_node(AST_WHILE, line, column);
    if (!node) return NULL;
    node->as.while_stmt.condition = condition;
    node->as.while_stmt.body = body;
    return node;
}

ASTNode* ast_for(const char* var_name, ASTNode* iterable, ASTNode* body,
                 int line, int column) {
    ASTNode* node = make_node(AST_FOR, line, column);
    if (!node) return NULL;
    node->as.for_stmt.var_name = var_name ? strdup(var_name) : NULL;
    node->as.for_stmt.iterable = iterable;
    node->as.for_stmt.body = body;
    return node;
}

// ===== Module =====

ASTNode* ast_module(int line, int column) {
    ASTNode* node = make_node(AST_MODULE, line, column);
    if (!node) return NULL;
    node->as.module.statements = NULL;
    node->as.module.count = 0;
    node->as.module.capacity = 0;
    return node;
}

void ast_module_add(ASTNode* module, ASTNode* statement) {
    if (!module || module->type != AST_MODULE || !statement) return;
    ASTModule* m = &module->as.module;
    if (m->count >= m->capacity) {
        int new_cap = m->capacity == 0 ? 8 : m->capacity * 2;
        m->statements = (ASTNode**)realloc(m->statements, sizeof(ASTNode*) * new_cap);
        m->capacity = new_cap;
    }
    m->statements[m->count++] = statement;
}

// ===== Destructor =====

void ast_destroy(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
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
        case AST_CALL:
            ast_destroy(node->as.call.callee);
            for (int i = 0; i < node->as.call.arg_count; i++) {
                ast_destroy(node->as.call.args[i]);
            }
            free(node->as.call.args);
            break;
        case AST_EXPRESSION_STMT:
            ast_destroy(node->as.expression_stmt.expression);
            break;
        case AST_ASSIGNMENT:
            ast_destroy(node->as.assignment.target);
            ast_destroy(node->as.assignment.value);
            break;
        case AST_RETURN:
            if (node->as.ret.value) ast_destroy(node->as.ret.value);
            break;
        case AST_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) {
                ast_destroy(node->as.block.statements[i]);
            }
            free(node->as.block.statements);
            break;
        case AST_IF:
            ast_destroy(node->as.if_stmt.condition);
            ast_destroy(node->as.if_stmt.then_block);
            if (node->as.if_stmt.else_block) ast_destroy(node->as.if_stmt.else_block);
            break;
        case AST_WHILE:
            ast_destroy(node->as.while_stmt.condition);
            ast_destroy(node->as.while_stmt.body);
            break;
        case AST_FOR:
            free(node->as.for_stmt.var_name);
            ast_destroy(node->as.for_stmt.iterable);
            ast_destroy(node->as.for_stmt.body);
            break;
        case AST_MODULE:
            for (int i = 0; i < node->as.module.count; i++) {
                ast_destroy(node->as.module.statements[i]);
            }
            free(node->as.module.statements);
            break;
    }

    free(node);
}

// ===== Debug =====

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
        case AST_CALL: return "Call";
        case AST_EXPRESSION_STMT: return "ExpressionStmt";
        case AST_ASSIGNMENT: return "Assignment";
        case AST_RETURN: return "Return";
        case AST_BLOCK: return "Block";
        case AST_IF: return "If";
        case AST_WHILE: return "While";
        case AST_FOR: return "For";
        case AST_MODULE: return "Module";
        default: return "UNKNOWN";
    }
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
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
        case AST_CALL:
            printf("Call\n");
            print_indent(indent + 1);
            printf("Callee:\n");
            ast_print(node->as.call.callee, indent + 2);
            print_indent(indent + 1);
            printf("Args(%d):\n", node->as.call.arg_count);
            for (int i = 0; i < node->as.call.arg_count; i++) {
                ast_print(node->as.call.args[i], indent + 2);
            }
            break;
        case AST_EXPRESSION_STMT:
            printf("ExpressionStmt\n");
            ast_print(node->as.expression_stmt.expression, indent + 1);
            break;
        case AST_ASSIGNMENT:
            printf("Assignment\n");
            ast_print(node->as.assignment.target, indent + 1);
            ast_print(node->as.assignment.value, indent + 1);
            break;
        case AST_RETURN:
            if (node->as.ret.value) {
                printf("Return\n");
                ast_print(node->as.ret.value, indent + 1);
            } else {
                printf("Return(bare)\n");
            }
            break;
        case AST_BLOCK:
            printf("Block(%d statements)\n", node->as.block.count);
            for (int i = 0; i < node->as.block.count; i++) {
                ast_print(node->as.block.statements[i], indent + 1);
            }
            break;
        case AST_IF:
            printf("If\n");
            print_indent(indent + 1);
            printf("Condition:\n");
            ast_print(node->as.if_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Then:\n");
            ast_print(node->as.if_stmt.then_block, indent + 2);
            if (node->as.if_stmt.else_block) {
                print_indent(indent + 1);
                printf("Else:\n");
                ast_print(node->as.if_stmt.else_block, indent + 2);
            }
            break;
        case AST_WHILE:
            printf("While\n");
            print_indent(indent + 1);
            printf("Condition:\n");
            ast_print(node->as.while_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->as.while_stmt.body, indent + 2);
            break;
        case AST_FOR:
            printf("For\n");
            print_indent(indent + 1);
            printf("Var: %s\n",
                   node->as.for_stmt.var_name ? node->as.for_stmt.var_name : "");
            print_indent(indent + 1);
            printf("Iterable:\n");
            ast_print(node->as.for_stmt.iterable, indent + 2);
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->as.for_stmt.body, indent + 2);
            break;
        case AST_MODULE:
            printf("Module(%d statements)\n", node->as.module.count);
            for (int i = 0; i < node->as.module.count; i++) {
                ast_print(node->as.module.statements[i], indent + 1);
            }
            break;
    }
}
