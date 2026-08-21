// types.c - Type system implementation for RHelix

#include "types.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// === Constructors ===

Type* type_create_primitive(TypeKind kind) {
    Type* t = (Type*)malloc(sizeof(Type));
    if (!t) return NULL;
    t->kind = kind;
    t->element_type = NULL;
    t->key_type = NULL;
    t->param_types = NULL;
    t->param_count = 0;
    t->return_type = NULL;
    return t;
}

Type* type_create_list(Type* element_type) {
    Type* t = type_create_primitive(TYPE_LIST);
    if (!t) return NULL;
    t->element_type = element_type;
    return t;
}

Type* type_create_dict(Type* key_type, Type* value_type) {
    Type* t = type_create_primitive(TYPE_DICT);
    if (!t) return NULL;
    t->key_type = key_type;
    t->element_type = value_type;
    return t;
}

Type* type_create_function(Type** param_types, int param_count, Type* return_type) {
    Type* t = type_create_primitive(TYPE_FUNCTION);
    if (!t) return NULL;
    t->param_types = param_types;
    t->param_count = param_count;
    t->return_type = return_type;
    return t;
}

// === Destructor ===

void type_destroy(Type* type) {
    if (!type) return;
    // Recursively free owned sub-types.
    type_destroy(type->element_type);
    type_destroy(type->key_type);
    type_destroy(type->return_type);
    if (type->param_types) {
        for (int i = 0; i < type->param_count; i++) {
            type_destroy(type->param_types[i]);
        }
        free(type->param_types);
    }
    free(type);
}

// === Conversion from AST ===

// Map an identifier name to a primitive TypeKind. Returns TYPE_ANY for
// unrecognized names (custom classes, etc.) - those are deferred.
static TypeKind primitive_kind_from_name(const char* name) {
    if (!name) return TYPE_ANY;
    if (strcmp(name, "int") == 0) return TYPE_INT;
    if (strcmp(name, "float") == 0) return TYPE_FLOAT;
    if (strcmp(name, "str") == 0) return TYPE_STRING;
    if (strcmp(name, "bool") == 0) return TYPE_BOOL;
    if (strcmp(name, "None") == 0) return TYPE_NONE;
    return TYPE_ANY;
}

Type* type_from_annotation(ASTNode* annotation) {
    if (!annotation) {
        return type_create_primitive(TYPE_ANY);
    }

    // Simple identifier annotation: int, str, bool, MyClass, etc.
    if (annotation->type == AST_IDENTIFIER) {
        const char* name = annotation->as.identifier.name;
        TypeKind kind = primitive_kind_from_name(name);
        return type_create_primitive(kind);
    }

    // Compound annotation: List[X], Dict[K, V], etc.
    // Represented in AST as Subscript(Identifier(base), index).
    if (annotation->type == AST_SUBSCRIPT) {
        ASTNode* base = annotation->as.subscript.object;
        ASTNode* index = annotation->as.subscript.index;

        if (base && base->type == AST_IDENTIFIER) {
            const char* base_name = base->as.identifier.name;

            if (strcmp(base_name, "List") == 0) {
                Type* element = type_from_annotation(index);
                return type_create_list(element);
            }
            if (strcmp(base_name, "Dict") == 0) {
                // Per current design: only the first type arg is preserved
                // in the AST for multi-arg generics. So Dict[str, int] parses
                // with just 'str' as the subscript index; the int is
                // discarded. We treat the single arg as the KEY type and
                // leave the value type as ANY. Revisit when multi-arg
                // generics are properly represented.
                Type* key = type_from_annotation(index);
                Type* value = type_create_primitive(TYPE_ANY);
                return type_create_dict(key, value);
            }
        }
    }

    // Anything we don't recognize - be permissive.
    return type_create_primitive(TYPE_ANY);
}

Type* type_of_literal(ASTNode* node) {
    if (!node) return NULL;
    switch (node->type) {
        case AST_LITERAL_INT:    return type_create_primitive(TYPE_INT);
        case AST_LITERAL_FLOAT:  return type_create_primitive(TYPE_FLOAT);
        case AST_LITERAL_STRING: return type_create_primitive(TYPE_STRING);
        case AST_LITERAL_BOOL:   return type_create_primitive(TYPE_BOOL);
        case AST_LITERAL_NONE:   return type_create_primitive(TYPE_NONE);
        default: return NULL;
    }
}

// === Debug ===

// Small helper: append 'src' to '*dest', growing as needed. Frees old dest
// content into the new buffer. Used by type_to_string to build compound
// type strings.
static char* str_append(char* dest, const char* src) {
    if (!dest) return strdup(src);
    size_t old_len = strlen(dest);
    size_t add_len = strlen(src);
    char* new_dest = (char*)realloc(dest, old_len + add_len + 1);
    if (!new_dest) return dest;
    memcpy(new_dest + old_len, src, add_len + 1);
    return new_dest;
}

char* type_to_string(Type* type) {
    if (!type) return strdup("<null>");

    switch (type->kind) {
        case TYPE_ANY:    return strdup("any");
        case TYPE_INT:    return strdup("int");
        case TYPE_FLOAT:  return strdup("float");
        case TYPE_STRING: return strdup("str");
        case TYPE_BOOL:   return strdup("bool");
        case TYPE_NONE:   return strdup("None");
        case TYPE_LIST: {
            char* inner = type_to_string(type->element_type);
            char* result = str_append(NULL, "List[");
            result = str_append(result, inner);
            result = str_append(result, "]");
            free(inner);
            return result;
        }
        case TYPE_DICT: {
            char* k = type_to_string(type->key_type);
            char* v = type_to_string(type->element_type);
            char* result = str_append(NULL, "Dict[");
            result = str_append(result, k);
            result = str_append(result, ", ");
            result = str_append(result, v);
            result = str_append(result, "]");
            free(k);
            free(v);
            return result;
        }
        case TYPE_FUNCTION: {
            char* result = str_append(NULL, "(");
            for (int i = 0; i < type->param_count; i++) {
                if (i > 0) result = str_append(result, ", ");
                char* p = type_to_string(type->param_types[i]);
                result = str_append(result, p);
                free(p);
            }
            result = str_append(result, ") -> ");
            char* ret = type_to_string(type->return_type);
            result = str_append(result, ret);
            free(ret);
            return result;
        }
        default: return strdup("<unknown>");
    }
}
