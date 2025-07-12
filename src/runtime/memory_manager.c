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
