// test_memory.c - Test suite for memory manager
#include "memory_manager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct {
    Object header;
    int value;
    char data[100];
} TestObject;

void test_reference_counting() {
    printf("Testing reference counting...\n");
    
    MemoryManager* mm = mm_create(1024 * 1024);  // 1MB heap
    
    // Allocate an object
    TestObject* obj = (TestObject*)mm_alloc(mm, sizeof(TestObject) - sizeof(Object));
    obj->value = 42;
    strcpy(obj->data, "Hello, RHelix!");
    
    printf("✓ Allocated object: value=%d, data=%s\n", obj->value, obj->data);
    printf("✓ Initial ref count: %u\n", obj->header.ref_count);
    
    // Test retain/release
    mm_retain((Object*)obj);
    assert(obj->header.ref_count == 2);
    printf("✓ After retain, ref count: %u\n", obj->header.ref_count);
    
    mm_release(mm, (Object*)obj);
    assert(obj->header.ref_count == 1);
    printf("✓ After release, ref count: %u\n", obj->header.ref_count);
    
    mm_release(mm, (Object*)obj);
    printf("✓ Object freed successfully\n");
    
    mm_destroy(mm);
    printf("✅ Reference counting tests passed!\n\n");
}

void test_arena_allocation() {
    printf("Testing arena allocation...\n");
    
    MemoryManager* mm = mm_create(1024 * 1024);
    Arena* arena = mm_arena_create(mm, 4096);  // 4KB arena
    
    // Allocate from arena
    int* numbers = (int*)mm_arena_alloc(arena, sizeof(int) * 100);
    char* buffer = (char*)mm_arena_alloc(arena, 256);
    
    // Use allocations
    for (int i = 0; i < 100; i++) {
        numbers[i] = i * i;
    }
    strcpy(buffer, "Arena allocated string");
    
    printf("✓ Arena allocations successful\n");
    printf("✓ Buffer content: %s\n", buffer);
    printf("✓ numbers[99] = %d\n", numbers[99]);
    
    mm_arena_reset(arena);
    printf("✓ Arena reset successful\n");
    
    mm_arena_destroy(mm, arena);
    mm_destroy(mm);
    printf("✅ Arena allocation tests passed!\n\n");
}

int main() {
    printf("=== RHelix Memory Manager Test Suite ===\n\n");
    
    test_reference_counting();
    test_arena_allocation();
    
    printf("🎉 All tests passed!\n");
    return 0;
}
