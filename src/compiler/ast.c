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

ASTNode* ast_subscript(ASTNode* object, ASTNode* index, int line, int column) {
    ASTNode* node = make_node(AST_SUBSCRIPT, line, column);
    if (!node) return NULL;
    node->as.subscript.object = object;
    node->as.subscript.index = index;
    return node;
}

ASTNode* ast_attribute(ASTNode* object, const char* name, int line, int column) {
    ASTNode* node = make_node(AST_ATTRIBUTE, line, column);
    if (!node) return NULL;
    node->as.attribute.object = object;
    node->as.attribute.name = name ? strdup(name) : NULL;
    return node;
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

ASTNode* ast_pass(int line, int column) {
    return make_node(AST_PASS, line, column);
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

// ===== Declaration constructors =====

ASTNode* ast_function_def(const char* name, ASTNode* return_type,
                          ASTNode* body, int line, int column) {
    ASTNode* node = make_node(AST_FUNCTION_DEF, line, column);
    if (!node) return NULL;
    node->as.function_def.name = name ? strdup(name) : NULL;
    node->as.function_def.params = NULL;
    node->as.function_def.param_count = 0;
    node->as.function_def.param_capacity = 0;
    node->as.function_def.return_type = return_type;
    node->as.function_def.body = body;
    node->as.function_def.decorators = NULL;
    node->as.function_def.decorator_count = 0;
    node->as.function_def.decorator_capacity = 0;
    return node;
}

void ast_function_def_add_param(ASTNode* func_def, const char* param_name,
                                ASTNode* type_annotation) {
    if (!func_def || func_def->type != AST_FUNCTION_DEF || !param_name) return;
    ASTFunctionDef* f = &func_def->as.function_def;
    if (f->param_count >= f->param_capacity) {
        int new_cap = f->param_capacity == 0 ? 4 : f->param_capacity * 2;
        f->params = (ASTParam*)realloc(f->params, sizeof(ASTParam) * new_cap);
        f->param_capacity = new_cap;
    }
    f->params[f->param_count].name = strdup(param_name);
    f->params[f->param_count].type_annotation = type_annotation;
    f->param_count++;
}

ASTNode* ast_class_def(const char* name, ASTNode* body, int line, int column) {
    ASTNode* node = make_node(AST_CLASS_DEF, line, column);
    if (!node) return NULL;
    node->as.class_def.name = name ? strdup(name) : NULL;
    node->as.class_def.base_classes = NULL;
    node->as.class_def.base_count = 0;
    node->as.class_def.base_capacity = 0;
    node->as.class_def.body = body;
    node->as.class_def.decorators = NULL;
    node->as.class_def.decorator_count = 0;
    node->as.class_def.decorator_capacity = 0;
    return node;
}

void ast_class_def_add_base(ASTNode* class_def, const char* base_name,
                            int line, int column) {
    if (!class_def || class_def->type != AST_CLASS_DEF || !base_name) return;
    ASTClassDef* c = &class_def->as.class_def;
    if (c->base_count >= c->base_capacity) {
        int new_cap = c->base_capacity == 0 ? 4 : c->base_capacity * 2;
        c->base_classes = (ASTNode**)realloc(c->base_classes,
                                             sizeof(ASTNode*) * new_cap);
        c->base_capacity = new_cap;
    }
    c->base_classes[c->base_count++] = ast_identifier(base_name, line, column);
}

void ast_function_def_add_decorator(ASTNode* func_def, ASTNode* decorator) {
    if (!func_def || func_def->type != AST_FUNCTION_DEF || !decorator) return;
    ASTFunctionDef* f = &func_def->as.function_def;
    if (f->decorator_count >= f->decorator_capacity) {
        int new_cap = f->decorator_capacity == 0 ? 4 : f->decorator_capacity * 2;
        f->decorators = (ASTNode**)realloc(f->decorators,
                                           sizeof(ASTNode*) * new_cap);
        f->decorator_capacity = new_cap;
    }
    f->decorators[f->decorator_count++] = decorator;
}

void ast_class_def_add_decorator(ASTNode* class_def, ASTNode* decorator) {
    if (!class_def || class_def->type != AST_CLASS_DEF || !decorator) return;
    ASTClassDef* c = &class_def->as.class_def;
    if (c->decorator_count >= c->decorator_capacity) {
        int new_cap = c->decorator_capacity == 0 ? 4 : c->decorator_capacity * 2;
        c->decorators = (ASTNode**)realloc(c->decorators,
                                           sizeof(ASTNode*) * new_cap);
        c->decorator_capacity = new_cap;
    }
    c->decorators[c->decorator_count++] = decorator;
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
        case AST_PASS:
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
        case AST_SUBSCRIPT:
            ast_destroy(node->as.subscript.object);
            ast_destroy(node->as.subscript.index);
            break;
        case AST_ATTRIBUTE:
            ast_destroy(node->as.attribute.object);
            free(node->as.attribute.name);
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
        case AST_FUNCTION_DEF:
            free(node->as.function_def.name);
            for (int i = 0; i < node->as.function_def.param_count; i++) {
                free(node->as.function_def.params[i].name);
                if (node->as.function_def.params[i].type_annotation) {
                    ast_destroy(node->as.function_def.params[i].type_annotation);
                }
            }
            free(node->as.function_def.params);
            if (node->as.function_def.return_type) {
                ast_destroy(node->as.function_def.return_type);
            }
            ast_destroy(node->as.function_def.body);
            for (int i = 0; i < node->as.function_def.decorator_count; i++) {
                ast_destroy(node->as.function_def.decorators[i]);
            }
            free(node->as.function_def.decorators);
            break;
        case AST_CLASS_DEF:
            free(node->as.class_def.name);
            for (int i = 0; i < node->as.class_def.base_count; i++) {
                ast_destroy(node->as.class_def.base_classes[i]);
            }
            free(node->as.class_def.base_classes);
            ast_destroy(node->as.class_def.body);
            for (int i = 0; i < node->as.class_def.decorator_count; i++) {
                ast_destroy(node->as.class_def.decorators[i]);
            }
            free(node->as.class_def.decorators);
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
        case AST_SUBSCRIPT: return "Subscript";
        case AST_ATTRIBUTE: return "Attribute";
        case AST_EXPRESSION_STMT: return "ExpressionStmt";
        case AST_ASSIGNMENT: return "Assignment";
        case AST_RETURN: return "Return";
        case AST_PASS: return "Pass";
        case AST_BLOCK: return "Block";
        case AST_IF: return "If";
        case AST_WHILE: return "While";
        case AST_FOR: return "For";
        case AST_FUNCTION_DEF: return "FunctionDef";
        case AST_CLASS_DEF: return "ClassDef";
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
        case AST_SUBSCRIPT:
            printf("Subscript\n");
            print_indent(indent + 1);
            printf("Object:\n");
            ast_print(node->as.subscript.object, indent + 2);
            print_indent(indent + 1);
            printf("Index:\n");
            ast_print(node->as.subscript.index, indent + 2);
            break;
        case AST_ATTRIBUTE:
            printf("Attribute(.%s)\n",
                   node->as.attribute.name ? node->as.attribute.name : "");
            print_indent(indent + 1);
            printf("Object:\n");
            ast_print(node->as.attribute.object, indent + 2);
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
        case AST_PASS:
            printf("Pass\n");
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
        case AST_FUNCTION_DEF:
            printf("FunctionDef(%s)\n",
                   node->as.function_def.name ? node->as.function_def.name : "");
            if (node->as.function_def.decorator_count > 0) {
                print_indent(indent + 1);
                printf("Decorators(%d):\n", node->as.function_def.decorator_count);
                for (int i = 0; i < node->as.function_def.decorator_count; i++) {
                    ast_print(node->as.function_def.decorators[i], indent + 2);
                }
            }
            print_indent(indent + 1);
            printf("Params(%d):\n", node->as.function_def.param_count);
            for (int i = 0; i < node->as.function_def.param_count; i++) {
                print_indent(indent + 2);
                printf("Param(%s)\n", node->as.function_def.params[i].name);
                if (node->as.function_def.params[i].type_annotation) {
                    print_indent(indent + 3);
                    printf("Type:\n");
                    ast_print(node->as.function_def.params[i].type_annotation,
                              indent + 4);
                }
            }
            if (node->as.function_def.return_type) {
                print_indent(indent + 1);
                printf("ReturnType:\n");
                ast_print(node->as.function_def.return_type, indent + 2);
            }
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->as.function_def.body, indent + 2);
            break;
        case AST_CLASS_DEF:
            printf("ClassDef(%s)\n",
                   node->as.class_def.name ? node->as.class_def.name : "");
            if (node->as.class_def.decorator_count > 0) {
                print_indent(indent + 1);
                printf("Decorators(%d):\n", node->as.class_def.decorator_count);
                for (int i = 0; i < node->as.class_def.decorator_count; i++) {
                    ast_print(node->as.class_def.decorators[i], indent + 2);
                }
            }
            if (node->as.class_def.base_count > 0) {
                print_indent(indent + 1);
                printf("Bases(%d):\n", node->as.class_def.base_count);
                for (int i = 0; i < node->as.class_def.base_count; i++) {
                    ast_print(node->as.class_def.base_classes[i], indent + 2);
                }
            }
            print_indent(indent + 1);
            printf("Body:\n");
            ast_print(node->as.class_def.body, indent + 2);
            break;
        case AST_MODULE:
            printf("Module(%d statements)\n", node->as.module.count);
            for (int i = 0; i < node->as.module.count; i++) {
                ast_print(node->as.module.statements[i], indent + 1);
            }
            break;
    }
}
