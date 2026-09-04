// types.h - Type system for RHelix
//
// The Type struct represents an inferred or declared type in the RHelix
// language. Types are used by the semantic analyzer for type checking and
// (eventually) by the code generator for specialization.
//
// Ownership model: Types are allocated per-symbol and freed with their
// owning symbol. No sharing, no reference counting - simpler than the
// alternatives at this scale.
//
// Compound types (list, dict, function) contain pointers to other Type
// instances that they own. The destructor recursively frees them.

#ifndef RHELIX_TYPES_H
#define RHELIX_TYPES_H

#include <stdbool.h>

typedef enum {
    TYPE_ANY,      // Unknown or unresolved - the "escape hatch"
    TYPE_INT,      // Integer literals, arithmetic on ints
    TYPE_FLOAT,    // Float literals, arithmetic involving floats
    TYPE_STRING,   // String literals, string operations
    TYPE_BOOL,     // Boolean literals and comparison results
    TYPE_NONE,     // None literal, void-return functions
    TYPE_LIST,     // Parameterized: element_type is the element type
    TYPE_EMPTY_LIST,  // Empty list literal []; matches any LIST in type_equals
    TYPE_DICT,     // Parameterized: key_type is the key, element_type is the value
    TYPE_EMPTY_DICT,  // Empty dict literal {}; matches any DICT in type_equals
    TYPE_FUNCTION  // Parameterized: param_types + return_type
} TypeKind;

typedef struct Type {
    TypeKind kind;

    // === Compound type payload ===
    // For LIST: element_type is the element type, key_type unused
    // For DICT: element_type is the value type, key_type is the key type
    struct Type* element_type;
    struct Type* key_type;

    // === Function type payload ===
    // For FUNCTION: param_types is an array of param_count Type pointers,
    // return_type is the function's return type
    struct Type** param_types;
    int param_count;
    struct Type* return_type;
} Type;

// === Constructors ===

// Create a primitive type (INT, FLOAT, STRING, BOOL, NONE, ANY).
// Panics if called with a compound TypeKind.
Type* type_create_primitive(TypeKind kind);

// Create a list type with the given element type. Takes ownership of element_type.
Type* type_create_list(Type* element_type);

// Create a dict type with the given key and value types. Takes ownership of both.
Type* type_create_dict(Type* key_type, Type* value_type);

// Create a function type. Takes ownership of param_types array and return_type.
// param_types should be a heap-allocated array of Type* of length param_count.
Type* type_create_function(Type** param_types, int param_count, Type* return_type);

// === Destructor ===

// Recursively frees a Type and all its owned sub-types. Safe to call with NULL.
void type_destroy(Type* type);

// === Conversion from AST ===

// Convert an annotation AST node (an Identifier or Subscript) into a Type.
// Returns TYPE_ANY for unrecognized identifiers (user-defined types deferred).
// Returns TYPE_ANY when annotation is NULL (no annotation was written).
// Caller owns the returned Type.
struct ASTNode;  // Forward declaration to avoid circular include
Type* type_from_annotation(struct ASTNode* annotation);

// Infer the type of a literal AST node. Returns NULL if node is not a literal.
// Caller owns the returned Type.
Type* type_of_literal(struct ASTNode* node);

// === Debug ===

// Return a heap-allocated string representation of the type, suitable for
// printing. Caller must free() the returned string.
// Examples: "int", "List[int]", "Dict[str, int]", "(int, str) -> bool"
// === Cloning and comparison ===

// Deep-copy a Type. All owned sub-types are cloned recursively. Returns
// NULL if input is NULL. Caller owns the returned Type.
Type* type_clone(Type* type);

// Structural equality of two types. Kinds must match and compound children
// must recursively equal. NULL types are equal to NULL, unequal to any
// non-NULL. TYPE_ANY equals TYPE_ANY (the caller decides whether ANY should
// be treated as a wildcard match; type_equals is strict).
bool type_equals(Type* a, Type* b);

char* type_to_string(Type* type);


#endif // RHELIX_TYPES_H
