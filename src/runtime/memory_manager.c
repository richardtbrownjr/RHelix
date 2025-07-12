// memory_manager.c
#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Create a new memory manager
MemoryManager* mm_create(size_t max_heap_size) {
    MemoryManager* mm = (MemoryManager*)calloc(1, sizeof(MemoryManager));
    if (!mm) return NULL;
    
    mm->max_heap_size = max_heap_size;
    mm->gc_threshold = max_heap_size / 10;  // GC when 10% of heap used
    
    return mm;
}

// Destroy memory manager and all its resources
void mm_destroy(MemoryManager* mm) {
    if (!mm) return;
    
    // Clean up all arenas
    Arena* arena = mm->arena_list;
    while (arena) {
        Arena* next = arena->next;
        free(arena->start);
        free(arena);
        arena = next;
    }
    
    free(mm);
}

// Allocate with automatic reference counting
Object* mm_alloc(MemoryManager* mm, size_t size) {
    // Check memory limits
    if (mm->allocated_bytes + size + sizeof(Object) > mm->max_heap_size) {
        // Try cycle collection first
        mm_collect_cycles(mm);
        
        if (mm->allocated_bytes + size + sizeof(Object) > mm->max_heap_size) {
            fprintf(stderr, "Out of memory: requested %zu bytes\n", size);
            return NULL;
        }
    }
    
    // Allocate object with header
    Object* obj = (Object*)calloc(1, sizeof(Object) + size);
    if (!obj) return NULL;
    
    obj->ref_count = 1;  // Start with reference count of 1
    obj->size = size;
    obj->flags = 0;
    
    // Update statistics
    mm->allocated_bytes += sizeof(Object) + size;
    mm->allocation_count++;
    mm->total_allocated += sizeof(Object) + size;
    
    // Check if we should run cycle detection
    if (mm->allocated_bytes > mm->gc_threshold) {
        mm_collect_cycles(mm);
    }
    
    return obj;
}

// Increment reference count
void mm_retain(Object* obj) {
    if (!obj || (obj->flags & OBJ_IMMORTAL)) return;
    
    obj->ref_count++;
}

// Decrement reference count and free if zero
void mm_release(MemoryManager* mm, Object* obj) {
    if (!obj || (obj->flags & OBJ_IMMORTAL)) return;
    
    assert(obj->ref_count > 0);
    obj->ref_count--;
    
    if (obj->ref_count == 0) {
        // Free the object
        mm->allocated_bytes -= sizeof(Object) + obj->size;
        mm->allocation_count--;
        mm->total_freed += sizeof(Object) + obj->size;
        
        free(obj);
    }
}
// Create a new arena for fast allocation
Arena* mm_arena_create(MemoryManager* mm, size_t size) {
    Arena* arena = (Arena*)calloc(1, sizeof(Arena));
    if (!arena) return NULL;
    
    arena->start = (char*)malloc(size);
    if (!arena->start) {
        free(arena);
        return NULL;
    }
    
    arena->current = arena->start;
    arena->end = arena->start + size;
    arena->chunk_size = size;
    
    // Add to arena list
    arena->next = mm->arena_list;
    mm->arena_list = arena;
    
    return arena;
}

// Allocate from arena (no individual frees)
void* mm_arena_alloc(Arena* arena, size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    if (arena->current + size > arena->end) {
        // Arena is full
        return NULL;
    }
    
    void* ptr = arena->current;
    arena->current += size;
    
    return ptr;
}

// Reset arena (free all at once)
void mm_arena_reset(Arena* arena) {
    arena->current = arena->start;
}

// Destroy arena
void mm_arena_destroy(MemoryManager* mm, Arena* arena) {
    if (!arena) return;
    
    // Remove from arena list
    Arena** prev = &mm->arena_list;
    while (*prev && *prev != arena) {
        prev = &(*prev)->next;
    }
    
    if (*prev) {
        *prev = arena->next;
    }
    
    free(arena->start);
    free(arena);
}

// Simple cycle detection (mark and sweep for cycles)
void mm_collect_cycles(MemoryManager* mm) {
    mm->gc_cycles++;
    
    // For now, just log that we would collect cycles
    if (mm->gc_cycles % 100 == 0) {
        fprintf(stderr, "GC: %zu cycles run, %zu bytes allocated\n", 
                mm->gc_cycles, mm->allocated_bytes);
    }
}

// Get current allocated bytes
size_t mm_get_allocated_bytes(MemoryManager* mm) {
    return mm->allocated_bytes;
}

// Print memory statistics
void mm_print_stats(MemoryManager* mm) {
    printf("Memory Statistics:\n");
    printf("  Currently allocated: %zu bytes in %zu objects\n", 
           mm->allocated_bytes, mm->allocation_count);
    printf("  Total allocated: %zu bytes\n", mm->total_allocated);
    printf("  Total freed: %zu bytes\n", mm->total_freed);
    printf("  GC cycles: %zu\n", mm->gc_cycles);
    printf("  Max heap size: %zu bytes\n", mm->max_heap_size);
    
    // Count arenas
    size_t arena_count = 0;
    size_t arena_bytes = 0;
    Arena* arena = mm->arena_list;
    while (arena) {
        arena_count++;
        arena_bytes += arena->chunk_size;
        arena = arena->next;
    }
    
    printf("  Arenas: %zu using %zu bytes\n", arena_count, arena_bytes);
}
