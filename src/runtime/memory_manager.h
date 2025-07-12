// memory_manager.h
#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct Object Object;
typedef struct Arena Arena;
typedef struct MemoryManager MemoryManager;

// Object header for all managed objects
typedef struct Object {
    uint32_t ref_count;      // Reference count for automatic management
    uint16_t flags;          // GC flags, object type, etc.
    uint16_t size;           // Size of allocation
    struct Object* next;     // For free list or GC traversal
    // Actual object data follows this header
} Object;

// Object flags
#define OBJ_MARKED    0x0001  // For cycle detection
#define OBJ_IMMORTAL  0x0002  // Never free this object
#define OBJ_ARENA     0x0004  // Allocated in arena
#define OBJ_STACK     0x0008  // Stack allocated

// Arena allocator for performance-critical sections
typedef struct Arena {
    char* start;
    char* current;
    char* end;
    struct Arena* next;
    size_t chunk_size;
} Arena;

// Main memory manager
typedef struct MemoryManager {
    // Reference counting
    Object* root_set;        // Root objects for cycle detection
    size_t allocated_bytes;
    size_t allocation_count;
    
    // Arena allocators
    Arena* current_arena;
    Arena* arena_list;
    
    // Memory limits
    size_t max_heap_size;
    size_t gc_threshold;
    
    // Statistics
    size_t total_allocated;
    size_t total_freed;
    size_t gc_cycles;
} MemoryManager;

// Core API
MemoryManager* mm_create(size_t max_heap_size);
void mm_destroy(MemoryManager* mm);

// Automatic memory management (default)
Object* mm_alloc(MemoryManager* mm, size_t size);
void mm_retain(Object* obj);
void mm_release(MemoryManager* mm, Object* obj);

// Arena allocation for performance
Arena* mm_arena_create(MemoryManager* mm, size_t size);
void* mm_arena_alloc(Arena* arena, size_t size);
void mm_arena_reset(Arena* arena);
void mm_arena_destroy(MemoryManager* mm, Arena* arena);

// Stack allocation helper
#define STACK_ALLOC(type, name, count) \
    type name[count]; \
    memset(name, 0, sizeof(type) * (count))

// Cycle detection and collection
void mm_collect_cycles(MemoryManager* mm);

// Memory introspection
size_t mm_get_allocated_bytes(MemoryManager* mm);
void mm_print_stats(MemoryManager* mm);

#endif // MEMORY_MANAGER_H
