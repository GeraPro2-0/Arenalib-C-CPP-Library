/*
 * Copyright (c) 2026 GeraPro2_0
 *
 * Licence: MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ARENALIB_H
#define ARENALIB_H

#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || defined(__cplusplus) || defined(_MSC_VER)
    #include <stddef.h>
    #include <stdint.h>
    typedef uintptr_t arenalib_uintptr_t;
    typedef size_t    arenalib_size_t;
#else
    #if !defined(_STDINT_H) && !defined(_STDINT_H_) && !defined(_GCC_STDINT_H) && !defined(__stdint_h__)
        #define _STDINT_H
        #define _STDINT_H_
        #define _GCC_STDINT_H
        #define __stdint_h__

        typedef unsigned char      uint8_t;
        typedef unsigned short     uint16_t;
        typedef unsigned int       uint32_t;
        #if defined(__GNUC__) || defined(__clang__)
            typedef unsigned long long uint64_t;
        #else
            typedef unsigned __int64   uint64_t;
        #endif
    #endif

    #if defined(_M_I86) || defined(__MSDOS__)
        typedef unsigned long arenalib_uintptr_t; 
    #elif defined(__alpha__) || defined(__ia64__) || defined(__x86_64__) || defined(_M_X64)
        typedef unsigned long long arenalib_uintptr_t;
    #else
        typedef unsigned long arenalib_uintptr_t;
    #endif
    
    #ifndef _SIZE_T_DEFINED
        typedef unsigned int arenalib_size_t;
    #else
        typedef size_t arenalib_size_t;
    #endif
#endif

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void *)0)
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Compiler compatibility macros */
#if defined(_MSC_VER)
    #define ARENALIB_INLINE static __inline
    #define ARENALIB_FORCE_INLINE static __forceinline
    #define ARENALIB_ASSUME(expr) __assume(expr)
#elif defined(__GNUC__) || defined(__clang__)
    #define ARENALIB_INLINE static inline __attribute__((always_inline))
    #define ARENALIB_FORCE_INLINE static inline __attribute__((always_inline))
    #define ARENALIB_ASSUME(expr) do { if (!(expr)) __builtin_unreachable(); } while (0)
#else
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
        #define ARENALIB_INLINE static inline
    #else
        #define ARENALIB_INLINE static
    #endif
    #define ARENALIB_FORCE_INLINE ARENALIB_INLINE
    #define ARENALIB_ASSUME(expr) ((void)0)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define ARENALIB_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define ARENALIB_CONCAT2(a, b) a##b
    #define ARENALIB_CONCAT(a, b) ARENALIB_CONCAT2(a, b)
    #define ARENALIB_STATIC_ASSERT(cond, msg) typedef char ARENALIB_CONCAT(arenalib_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* Feature detection: use compiler builtins, standard C11 atomics, or a fallback. */
#if defined(__GNUC__) || defined(__clang__)
    #define ARENALIB_HAS_ATOMICS 1
    #define ARENALIB_ATOMIC(type) type
    #define ARENALIB_ATOMIC_LOAD(ptr) __atomic_load_n((ptr), __ATOMIC_SEQ_CST)
    #define ARENALIB_ATOMIC_CAS(ptr, expected, desired) \
        __atomic_compare_exchange_n((ptr), (expected), (desired), 1, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)
    #define ARENALIB_ATOMIC_CLEAR(ptr, mask) __atomic_and_fetch((ptr), (mask), __ATOMIC_SEQ_CST)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__) && !defined(_MSC_VER)
    #include <stdatomic.h>
    #define ARENALIB_HAS_ATOMICS 1
    #define ARENALIB_ATOMIC(type) _Atomic(type)
    #define ARENALIB_ATOMIC_LOAD(ptr) atomic_load_explicit((ptr), memory_order_seq_cst)
    #define ARENALIB_ATOMIC_CAS(ptr, expected, desired) \
        atomic_compare_exchange_strong_explicit((ptr), (expected), (desired), memory_order_seq_cst, memory_order_relaxed)
    #define ARENALIB_ATOMIC_CLEAR(ptr, mask) atomic_fetch_and_explicit((ptr), (mask), memory_order_seq_cst)
#else
    #define ARENALIB_HAS_ATOMICS 0
    #define ARENALIB_ATOMIC(type) type
#endif

/* Default alignment used by arena allocations. */
#ifndef ARENALIB_DEFAULT_ALIGNMENT
    #define ARENALIB_DEFAULT_ALIGNMENT (sizeof(void *))
#endif

typedef void (*arenalib_oom_callback_t)(void *user_data, arenalib_size_t size_requested);

/* Structure for secure IDs (32-bit index, 16-bit generation) */
typedef struct arenalib_id_t {
    uint32_t index;
    uint16_t generation;
} arenalib_id_t;

/* Portable invalid ID provider: prefer function over compound-literal macro
 * to avoid C++ pedantic warnings about compound literals. */
ARENALIB_INLINE arenalib_id_t arenalib_invalid_id(void) {
    arenalib_id_t id;
    id.index = 0xFFFFFFFFu;
    id.generation = (uint16_t)0xFFFFu;
    return id;
}

/* Function form is valid in C89, C99, C11, and C++. */
#define ARENALIB_INVALID_ID arenalib_invalid_id()

/* Static Pool Configuration */
#ifndef ARENALIB_POOL_BLOCKS
    #define ARENALIB_POOL_BLOCKS 64
#endif
#ifndef ARENALIB_POOL_BLOCK_SIZE
    #define ARENALIB_POOL_BLOCK_SIZE 1048576
#endif

typedef struct arenalib_arena_t {
    arenalib_size_t capacity;
    arenalib_size_t used;
    arenalib_size_t last_size;
    unsigned char *data;
    void *storage;
    arenalib_oom_callback_t oom_callback;
    void *oom_user_data;

    uint16_t *generations;
    uint32_t *offsets;
    uint32_t *free_next;
    uint32_t max_items;
    uint32_t item_count;
    uint32_t free_head;

    struct arenalib_arena_t *next;
    arenalib_size_t block_alloc_size;
    int is_dynamic;
    int pool_index;
} arenalib_arena_t;

typedef union {
    arenalib_arena_t dummy_alignment;
    unsigned char storage[ARENALIB_POOL_BLOCK_SIZE];
} arenalib_pool_block_t;

/* Static global structures of the Producer */
static arenalib_pool_block_t g_arena_pool[ARENALIB_POOL_BLOCKS];
static ARENALIB_ATOMIC(uint64_t) g_pool_bitmap = 0;

ARENALIB_STATIC_ASSERT(sizeof(void *) == ARENALIB_DEFAULT_ALIGNMENT, "Arenalib assumes pointer-sized alignment by default");
ARENALIB_STATIC_ASSERT(ARENALIB_POOL_BLOCKS <= 64, "Arenalib pool bitmap supports at most 64 blocks");

/* Safely align up with overflow detection */
ARENALIB_INLINE arenalib_size_t arenalib_align_up(arenalib_size_t value, arenalib_size_t alignment) {
    arenalib_size_t aligned;
    if (alignment == 0) {
        return value;
    }
    /* Detect overflow: if value + (alignment - 1) wraps around */
    if (value > (arenalib_size_t)-1 - (alignment - 1)) {
        return 0; /* Overflow detected, return 0 as sentinel */
    }
    aligned = ((value + alignment - 1) / alignment) * alignment;
    /* Verify result didn't overflow or wrap */
    if (aligned < value) {
        return 0; /* Multiplication overflow */
    }
    return aligned;
}

ARENALIB_INLINE void *arenalib_align_ptr(void *ptr, arenalib_size_t alignment) {
    arenalib_uintptr_t value = (arenalib_uintptr_t)ptr;
    value = arenalib_align_up(value, alignment);
    return (void *)value;
}

ARENALIB_INLINE void arenalib_memcpy(void *dest, const void *src, arenalib_size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (count--) {
        *d++ = *s++;
    }
}

ARENALIB_INLINE void arenalib_memset(void *dest, int byte_value, arenalib_size_t count) {
    unsigned char *d = (unsigned char *)dest;
    unsigned char value = (unsigned char)byte_value;
    while (count--) {
        *d++ = value;
    }
}

ARENALIB_INLINE int arenalib_size_mul(arenalib_size_t left, arenalib_size_t right, arenalib_size_t *result) {
    if (!result || (right != 0 && left > (arenalib_size_t)-1 / right)) {
        return 0;
    }
    *result = left * right;
    return 1;
}

/*
 * Initialize an arena from a backing buffer.
 *
 * storage: pointer to the buffer that will back the arena.
 * capacity: size of the buffer in bytes.
 * max_ids: maximum number of IDs this arena can keep track of simultaneously. Pass 0 if ID features aren't needed.
 *
 * The caller owns the backing storage; destroy does not free it.
 */
ARENALIB_INLINE int arenalib_arena_init(arenalib_arena_t *arena, void *storage, arenalib_size_t capacity, uint32_t max_ids) {
    void *aligned_data;
    arenalib_uintptr_t adjustment;
    arenalib_size_t per_item;
    arenalib_size_t meta_size;
    arenalib_size_t aligned_meta;
    arenalib_uintptr_t offsets_addr;
    uint32_t i;
    if (!arena || !storage || capacity == 0) {
        return 0;
    }

    aligned_data = arenalib_align_ptr(storage, ARENALIB_DEFAULT_ALIGNMENT);
    adjustment = (arenalib_uintptr_t)aligned_data - (arenalib_uintptr_t)storage;
    /* Defensive: ensure adjustment doesn't exceed capacity and capacity is reasonable */
    if (adjustment > capacity) {
        return 0;
    }

    arena->capacity = capacity - adjustment;
    arena->used = 0;
    arena->last_size = 0;
    arena->data = (unsigned char *)aligned_data;
    arena->storage = storage;
    arena->oom_callback = NULL;
    arena->oom_user_data = NULL;
    
    arena->max_items = max_ids; 
    arena->item_count = 0;
    
    if (max_ids > 0) {
        /* Detect multiplication overflow in metadata size calculation */
        per_item = sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t);
        if (max_ids > (arenalib_size_t)-1 / per_item) {
            return 0; /* Overflow in meta_size calculation */
        }
        meta_size = (arenalib_size_t)max_ids * per_item;
        /* Ensure that the metadata at the end of the arena maintains the correct alignment */
        aligned_meta = arenalib_align_up(meta_size, ARENALIB_DEFAULT_ALIGNMENT);
        if (aligned_meta == 0) {
            return 0; /* Overflow in alignment calculation */
        }
        meta_size = aligned_meta;
        
        if (arena->capacity <= meta_size) {
            return 0;
        }
        
        arena->generations = (uint16_t*)(arena->data + arena->capacity - meta_size);
        /* Validate generations pointer is within allocated memory */
        if (arena->generations < (uint16_t*)arena->data) {
            return 0; /* Pointer calculation failed */
        }
        offsets_addr = (arenalib_uintptr_t)(arena->generations + arena->max_items);
        arena->offsets = (uint32_t*)arenalib_align_ptr((void*)offsets_addr, sizeof(uint32_t));
        arena->free_next = arena->offsets + arena->max_items;
        /* Validate metadata pointers are within the metadata region */
        if (arena->offsets < (uint32_t*)arena->generations || 
            arena->free_next < arena->offsets ||
            arena->free_next + arena->max_items > (uint32_t*)(arena->data + arena->capacity)) {
            return 0; /* Metadata pointers out of bounds */
        }

        
        arenalib_memset(arena->generations, 0, arena->max_items * sizeof(uint16_t));
        arenalib_memset(arena->offsets, 0xFF, arena->max_items * sizeof(uint32_t));
        
        {
            for (i = 0; i < arena->max_items - 1; i++) {
                arena->free_next[i] = i + 1;
            }
            arena->free_next[arena->max_items - 1] = 0xFFFFFFFF;
        }
        arena->free_head = 0;
        
        arena->capacity -= meta_size;
    } else {
        arena->generations = NULL;
        arena->offsets = NULL;
        arena->free_next = NULL;
        arena->free_head = 0xFFFFFFFF;
    }
    
    arena->next = NULL;
    arena->block_alloc_size = capacity;
    arena->is_dynamic = 0;
    arena->pool_index = -1;
    
    return 1;
}

/* Atomic function of the Producer to dispatch free blocks from the static pool inheriting the capacity of IDs */
ARENALIB_INLINE int arenalib_pool_acquire_block(uint32_t max_ids) {
    int free_bit = -1;
    int i;
    uint64_t next_bitmap;
    int init_result;
    arenalib_arena_t *allocated_arena;

#if ARENALIB_HAS_ATOMICS || defined(__GNUC__) || defined(__clang__)
    /* ARM requires SEQ_CST for reliable memory ordering; use it for bitmap loads */
    uint64_t current_bitmap = ARENALIB_ATOMIC_LOAD(&g_pool_bitmap);
    while (1) {
        free_bit = -1;
        for (i = 0; i < ARENALIB_POOL_BLOCKS; i++) {
            if (!(current_bitmap & ((uint64_t)1 << i))) {
                free_bit = i;
                break;
            }
        }
        if (free_bit == -1) return -1;

        next_bitmap = current_bitmap | ((uint64_t)1 << free_bit);
        if (ARENALIB_ATOMIC_CAS(&g_pool_bitmap, &current_bitmap, next_bitmap)) {
            break;
        }
    }
#else
    uint64_t current_bitmap = g_pool_bitmap;
    for (i = 0; i < ARENALIB_POOL_BLOCKS; i++) {
        if (!(current_bitmap & ((uint64_t)1 << i))) {
            free_bit = i;
            break;
        }
    }
        if (free_bit == -1) return -1;
    g_pool_bitmap |= ((uint64_t)1 << free_bit);
#endif

    if (free_bit == -1 || free_bit >= ARENALIB_POOL_BLOCKS) {
        return -1;
    }

    /* Defensive: verify bounds before initialization */
    if (ARENALIB_POOL_BLOCK_SIZE <= sizeof(arenalib_arena_t)) {
        uint64_t mask = ~((uint64_t)1 << free_bit);
    #if ARENALIB_HAS_ATOMICS || defined(__GNUC__) || defined(__clang__)
        ARENALIB_ATOMIC_CLEAR(&g_pool_bitmap, mask);
    #else
        g_pool_bitmap &= mask;
    #endif
        return -1; /* Pool block too small for metadata */
    }
    
    init_result = arenalib_arena_init((arenalib_arena_t*)&g_arena_pool[free_bit].storage[0], 
                        (void*)&g_arena_pool[free_bit].storage[sizeof(arenalib_arena_t)], 
                        ARENALIB_POOL_BLOCK_SIZE - sizeof(arenalib_arena_t), max_ids);
    
    if (!init_result) {
        /* Roll back bitmap on init failure */
        if (free_bit >= 0 && free_bit < ARENALIB_POOL_BLOCKS) {
            uint64_t mask = ~((uint64_t)1 << free_bit);
#if ARENALIB_HAS_ATOMICS || defined(__GNUC__) || defined(__clang__)
            ARENALIB_ATOMIC_CLEAR(&g_pool_bitmap, mask);
#else
            g_pool_bitmap &= mask;
#endif
        }
        return -1;
    }
    
    allocated_arena = (arenalib_arena_t*)&g_arena_pool[free_bit].storage[0];
    allocated_arena->pool_index = free_bit;
    return free_bit;
}

ARENALIB_INLINE void arenalib_arena_reset(arenalib_arena_t *arena) {
    arenalib_arena_t *current;
    if (!arena) {
        return;
    }

    current = arena->next;
    
    while (current != NULL) {
        arenalib_arena_t *next_block = current->next;
        int idx = current->pool_index;
        if (idx >= 0 && idx < ARENALIB_POOL_BLOCKS) {
            uint64_t mask = ~((uint64_t)1 << idx);
#if ARENALIB_HAS_ATOMICS || defined(__GNUC__) || defined(__clang__)
            ARENALIB_ATOMIC_CLEAR(&g_pool_bitmap, mask);
#else
            g_pool_bitmap &= mask;
#endif
        }
        current = next_block;
    }

    arena->used = 0;
    arena->last_size = 0;
    arena->item_count = 0;
    arena->next = NULL;
    
    if (arena->offsets && arena->max_items > 0) {
        uint32_t i;
        for (i = 0; i < arena->max_items; i++) {
            if (arena->offsets[i] != 0xFFFFFFFF) {
                arena->generations[i]++;
            }
            arena->offsets[i] = 0xFFFFFFFF;
        }
        for (i = 0; i < arena->max_items - 1; i++) {
            arena->free_next[i] = i + 1;
        }
        arena->free_next[arena->max_items - 1] = 0xFFFFFFFF;
        arena->free_head = 0;
    }
}

ARENALIB_INLINE arenalib_size_t arenalib_arena_available(const arenalib_arena_t *arena) {
    if (!arena) {
        return 0;
    }
    if (arena->capacity < arena->used) {
        return 0;
    }
    return arena->capacity - arena->used;
}

ARENALIB_INLINE void *arenalib_arena_malloc(arenalib_arena_t *arena, arenalib_size_t size) {
    arenalib_size_t aligned_size;
    arenalib_arena_t *current;
    int chain_depth;
    int block_idx;
    unsigned char *ptr;
    if (!arena || size == 0) {
        return NULL;
    }

    aligned_size = arenalib_align_up(size, ARENALIB_DEFAULT_ALIGNMENT);
    if (aligned_size == 0 || aligned_size > ARENALIB_POOL_BLOCK_SIZE) {
        return NULL; /* Overflow or size too large */
    }

    current = arena;
    
    /* Defensive: limit chain depth to prevent infinite loops */
    chain_depth = 0;
    while (current->next != NULL) {
        if (chain_depth++ > ARENALIB_POOL_BLOCKS) {
            if (arena->oom_callback) {
                arena->oom_callback(arena->oom_user_data, size);
            }
            return NULL; /* Corrupted chain */
        }
        current = current->next;
    }

    if (aligned_size > arenalib_arena_available(current)) {
        if (aligned_size > (ARENALIB_POOL_BLOCK_SIZE - sizeof(arenalib_arena_t))) {
            if (arena->oom_callback) {
                arena->oom_callback(arena->oom_user_data, aligned_size);
            }
            return NULL;
        }

        block_idx = arenalib_pool_acquire_block(0); 
        if (block_idx == -1 || block_idx >= ARENALIB_POOL_BLOCKS) {
            if (arena->oom_callback) {
                arena->oom_callback(arena->oom_user_data, aligned_size);
            }
            return NULL;
        }

        current->next = (arenalib_arena_t*)&g_arena_pool[block_idx].storage;
        current = current->next;
    }

    /* Defensive: check for overflow in used + aligned_size */
    if (current->used > current->capacity - aligned_size) {
        if (arena->oom_callback) {
            arena->oom_callback(arena->oom_user_data, aligned_size);
        }
        return NULL; /* Overflow detected */
    }
    
    ptr = current->data + current->used;
    current->used += aligned_size;
    current->last_size = aligned_size;
    return ptr;
}

ARENALIB_INLINE void *arenalib_arena_calloc(arenalib_arena_t *arena, arenalib_size_t count, arenalib_size_t size) {
    arenalib_size_t total;
    void *ptr;
    if (!arena || count == 0 || size == 0) {
        return NULL;
    }
    /* Detect multiplication overflow */
    if (!arenalib_size_mul(count, size, &total)) {
        return NULL;
    }
    ptr = arenalib_arena_malloc(arena, total);
    if (ptr) {
        arenalib_memset(ptr, 0, total);
    }
    return ptr;
}

ARENALIB_INLINE void arenalib_arena_free(arenalib_arena_t *arena, void *ptr, arenalib_size_t size) {
    arenalib_size_t aligned_size;
    arenalib_arena_t *current;
    int chain_depth;
    if (!arena || !ptr || size == 0) {
        return;
    }

    aligned_size = arenalib_align_up(size, ARENALIB_DEFAULT_ALIGNMENT);
    if (aligned_size == 0) return; /* Overflow in alignment */
    
    current = arena;
    chain_depth = 0;

    while (current != NULL) {
        /* Limit chain depth to prevent infinite loops */
        if (chain_depth++ > ARENALIB_POOL_BLOCKS) {
            return; /* Corrupted chain */
        }
        
        /* Defensive: verify data pointer and capacity are sane */
        if (!current->data || current->capacity == 0) {
            current = current->next;
            continue;
        }
        
        /* We check if the pointer falls within the memory range of this specific arena */
        if ((unsigned char *)ptr >= current->data && (unsigned char *)ptr < (current->data + current->capacity)) {
            /* Defensive: verify used >= last_size before subtraction */
            if (current->last_size == aligned_size && current->used >= current->last_size) {
                unsigned char *expected = current->data + (current->used - current->last_size);
                if ((unsigned char *)ptr == expected) {
                    current->used -= current->last_size;
                    current->last_size = 0;
                }
            }
            return;
        }
        current = current->next;
    }
}

ARENALIB_INLINE void *arenalib_arena_realloc(arenalib_arena_t *arena, void *ptr, arenalib_size_t old_size, arenalib_size_t new_size) {
    arenalib_size_t aligned_old;
    arenalib_size_t aligned_new;
    arenalib_size_t reduction;
    arenalib_size_t growth;
    arenalib_size_t copy_size;
    arenalib_arena_t *current;
    unsigned char *current_last;
    int chain_depth;
    void *new_ptr;
    if (!arena) return NULL;
    if (ptr == NULL) return arenalib_arena_malloc(arena, new_size);
    if (new_size == old_size) return ptr;

    aligned_old = arenalib_align_up(old_size, ARENALIB_DEFAULT_ALIGNMENT);
    aligned_new = arenalib_align_up(new_size, ARENALIB_DEFAULT_ALIGNMENT);
    
    /* Detect overflow in alignment calculations */
    if (aligned_old == 0 || aligned_new == 0) {
        return NULL;
    }

    current = arena;
    chain_depth = 0;
    
    while (current != NULL) {
        if (chain_depth++ > ARENALIB_POOL_BLOCKS) {
            return NULL; /* Corrupted chain */
        }
        
        /* Defensive: verify used >= last_size before subtraction */
        if (!current->data || current->capacity == 0 || current->used < current->last_size) {
            current = current->next;
            continue;
        }
        
        current_last = current->data + (current->used - current->last_size);
        
        if ((unsigned char *)ptr == current_last && aligned_old == current->last_size) {
            if (aligned_new <= aligned_old) {
                /* Shrinking: safe subtraction after bounds check */
                reduction = aligned_old - aligned_new;
                if (current->used >= reduction) {
                    current->used -= reduction;
                    current->last_size = aligned_new;
                    return ptr;
                }
            }
            if (aligned_new > aligned_old) {
                /* Expanding: check for overflow before adding */
                growth = aligned_new - aligned_old;
                if (growth <= arenalib_arena_available(current)) {
                    current->used += growth;
                    current->last_size = aligned_new;
                    return ptr;
                }
            }
            break;
        }
        current = current->next;
    }

    new_ptr = arenalib_arena_malloc(arena, new_size);
    if (!new_ptr) return NULL;

    copy_size = old_size < new_size ? old_size : new_size;
    arenalib_memcpy(new_ptr, ptr, copy_size);
    return new_ptr;
}

ARENALIB_INLINE void arenalib_arena_destroy(arenalib_arena_t *arena) {
    arenalib_arena_t *current;
    arenalib_arena_t *next_block;
    int idx;
    if (!arena) return;

    current = arena->next;
    
    while (current != NULL) {
        next_block = current->next;
        idx = current->pool_index;
        
        if (idx >= 0 && idx < ARENALIB_POOL_BLOCKS) {
            uint64_t mask = ~((uint64_t)1 << idx);
#if ARENALIB_HAS_ATOMICS || defined(__GNUC__) || defined(__clang__)
            ARENALIB_ATOMIC_CLEAR(&g_pool_bitmap, mask);
#else
            g_pool_bitmap &= mask;
#endif
        }
        current = next_block;
    }

    arena->capacity = 0;
    arena->used = 0;
    arena->last_size = 0;
    arena->data = NULL;
    arena->storage = NULL;
    arena->oom_callback = NULL;
    arena->oom_user_data = NULL;
    arena->generations = NULL;
    arena->offsets = NULL;
    arena->free_next = NULL;
    arena->max_items = 0;
    arena->item_count = 0;
    arena->free_head = 0xFFFFFFFF;
    arena->next = NULL;
    arena->pool_index = -1;
}

ARENALIB_INLINE void arenalib_arena_set_oom_callback(arenalib_arena_t *arena, arenalib_oom_callback_t callback, void *user_data) {
    if (arena) {
        arena->oom_callback = callback;
        arena->oom_user_data = user_data;
    }
}

ARENALIB_INLINE void *arenalib_arena_malloc_align(arenalib_arena_t *arena, arenalib_size_t size, arenalib_size_t alignment) {
    arenalib_size_t aligned_size;
    arenalib_arena_t *current;
    int chain_depth;
    unsigned char *current_ptr;
    void *aligned_ptr;
    arenalib_size_t adjustment;
    arenalib_size_t total_needed;
    int block_idx;
    if (!arena || size == 0 || alignment == 0) return NULL;

    aligned_size = arenalib_align_up(size, ARENALIB_DEFAULT_ALIGNMENT);
    if (aligned_size == 0) return NULL;

    current = arena;
    chain_depth = 0;
    while (current->next != NULL) {
        if (chain_depth++ > ARENALIB_POOL_BLOCKS) return NULL;
        current = current->next;
    }

    if (!current->data || current->used > current->capacity) return NULL;

    current_ptr = current->data + current->used;
    aligned_ptr = arenalib_align_ptr(current_ptr, alignment);
    if (!aligned_ptr) return NULL;

    adjustment = (arenalib_size_t)((unsigned char *)aligned_ptr - current_ptr);
    if (adjustment > (arenalib_size_t)-1 - aligned_size) return NULL;
    total_needed = adjustment + aligned_size;

    if (total_needed > arenalib_arena_available(current)) {
        if (total_needed > (ARENALIB_POOL_BLOCK_SIZE - sizeof(arenalib_arena_t))) {
            if (arena->oom_callback) {
                arena->oom_callback(arena->oom_user_data, total_needed);
            }
            return NULL;
        }

        block_idx = arenalib_pool_acquire_block(0);
        if (block_idx < 0 || block_idx >= ARENALIB_POOL_BLOCKS) {
            if (arena->oom_callback) {
                arena->oom_callback(arena->oom_user_data, total_needed);
            }
            return NULL;
        }

        current->next = (arenalib_arena_t *)&g_arena_pool[block_idx].storage;
        current = current->next;
        current_ptr = current->data + current->used;
        aligned_ptr = arenalib_align_ptr(current_ptr, alignment);
        if (!aligned_ptr) return NULL;
        adjustment = (arenalib_size_t)((unsigned char *)aligned_ptr - current_ptr);
        if (adjustment > (arenalib_size_t)-1 - aligned_size) return NULL;
        total_needed = adjustment + aligned_size;
    }

    if (total_needed > arenalib_arena_available(current)) {
        if (arena->oom_callback) {
            arena->oom_callback(arena->oom_user_data, total_needed);
        }
        return NULL;
    }

    current->used += total_needed;
    current->last_size = total_needed;
    return aligned_ptr;
}

typedef struct arenalib_marker_t {
    arenalib_arena_t *block;
    arenalib_size_t used;
} arenalib_marker_t;

ARENALIB_INLINE arenalib_marker_t arenalib_arena_get_marker(const arenalib_arena_t *arena) {
    arenalib_marker_t marker;
    marker.block = NULL;
    marker.used = 0;
    if (arena) {
        const arenalib_arena_t *current = arena;
        while (current->next != NULL) {
            current = current->next;
        }
        marker.block = (arenalib_arena_t *)current;
        marker.used = current->used;
    }
    return marker;
}

ARENALIB_INLINE void arenalib_arena_release_marker(arenalib_arena_t *arena, arenalib_marker_t marker) {
    if (arena && marker.block) {
        arenalib_arena_t *current = arena;
        arenalib_arena_t *marked_block = NULL;
        int chain_depth = 0;
        while (current != NULL) {
            if (current == marker.block) {
                marked_block = current;
                break;
            }
            if (chain_depth++ > ARENALIB_POOL_BLOCKS) return;
            current = current->next;
        }
        if (!marked_block || marker.used > marked_block->used) return;

        current = marked_block->next;
        while (current != NULL) {
            arenalib_arena_t *next_block = current->next;
            int idx = current->pool_index;
            if (idx >= 0 && idx < ARENALIB_POOL_BLOCKS) {
                uint64_t mask = ~((uint64_t)1 << idx);
#if ARENALIB_HAS_ATOMICS || defined(__GNUC__) || defined(__clang__)
                ARENALIB_ATOMIC_CLEAR(&g_pool_bitmap, mask);
#else
                g_pool_bitmap &= mask;
#endif
            }
            current = next_block;
        }
        marked_block->next = NULL;
        marked_block->used = marker.used;
        marked_block->last_size = 0;
        
        if (marked_block == arena && arena->offsets && arena->max_items > 0) {
            uint32_t i;
            for (i = 0; i < arena->max_items; i++) {
                if (arena->offsets[i] != 0xFFFFFFFF && arena->offsets[i] >= marker.used) {
                    arena->generations[i]++;
                    arena->offsets[i] = 0xFFFFFFFF;
                    arena->free_next[i] = arena->free_head;
                    arena->free_head = i;
                    if (arena->item_count > 0) {
                        arena->item_count--;
                    }
                }
            }
        }

    }
}

/* Allocates memory and returns a unique ID instead of a raw pointer */
ARENALIB_INLINE arenalib_id_t arenalib_arena_alloc_id(arenalib_arena_t *arena, arenalib_size_t size) {
    arenalib_size_t base_available;
    void *ptr;
    uint32_t slot;
    arenalib_id_t id;
    if (!arena || !arena->offsets || !arena->free_next ||
        arena->free_head == 0xFFFFFFFF || arena->free_head >= arena->max_items ||
        arena->next != NULL) {
        return arenalib_invalid_id();
    }

    /* ID offsets are relative to the first arena block. */
    base_available = arenalib_arena_available(arena);
    if (size == 0 || base_available < (ARENALIB_DEFAULT_ALIGNMENT - 1) ||
        size > base_available - (ARENALIB_DEFAULT_ALIGNMENT - 1)) {
        return arenalib_invalid_id();
    }

    ptr = arenalib_arena_malloc_align(arena, size, ARENALIB_DEFAULT_ALIGNMENT);
    if (!ptr) return arenalib_invalid_id();

    slot = arena->free_head;
    
    arena->free_head = arena->free_next[slot];
    
    arena->offsets[slot] = (uint32_t)((unsigned char*)ptr - arena->data);
    arena->item_count++;
    
    id.index = slot;
    id.generation = arena->generations[slot];
    
    return id;
}

/* Translates a secure ID to an actual memory pointer */
ARENALIB_INLINE void *arenalib_arena_get_ptr(arenalib_arena_t *arena, arenalib_id_t id) {
    uint32_t offset;
    if (!arena || !arena->offsets || !arena->generations || id.index >= arena->max_items) return NULL;
    
    /* Defensive: verify the offset doesn't exceed capacity */
    offset = arena->offsets[id.index];
    if (offset == 0xFFFFFFFF || offset >= arena->capacity) {
        return NULL; /* Invalid offset or ID is free */
    }
    
    if (arena->generations[id.index] != id.generation) return NULL;
    
    return (void*)(arena->data + offset);
}

/* Invalidates an ID by incrementing its generation and recycles its slot */
ARENALIB_INLINE void arenalib_arena_free_id(arenalib_arena_t *arena, arenalib_id_t id) {
    if (arena && arena->generations && arena->offsets && id.index < arena->max_items) {
        if (arena->generations[id.index] == id.generation &&
            arena->offsets[id.index] != 0xFFFFFFFF) {
            arena->generations[id.index]++;
            
            arena->offsets[id.index] = 0xFFFFFFFF;
            arena->free_next[id.index] = arena->free_head;
            arena->free_head = id.index;
            
            if (arena->item_count > 0) {
                arena->item_count--;
            }
        }
    }
}

#ifdef __cplusplus
}

namespace arenalib {
    typedef ::arenalib_arena_t arena_t;
    typedef ::arenalib_oom_callback_t oom_callback_t;
    typedef ::arenalib_marker_t marker_t;
    typedef ::arenalib_id_t id_t;

#if defined(__cpp_constexpr) || (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1900)
    static constexpr arenalib_size_t default_alignment = ARENALIB_DEFAULT_ALIGNMENT;
#else
    static const arenalib_size_t default_alignment = ARENALIB_DEFAULT_ALIGNMENT;
#endif

#if defined(__GNUC__) || defined(_MSC_VER) || (defined(__cplusplus) && __cplusplus >= 199711L)
    typedef bool arena_bool_t;
#else
    typedef int arena_bool_t;
#endif

    inline arena_bool_t init(arena_t *arena, void *storage, arenalib_size_t capacity, uint32_t max_ids = 0) {
        return ::arenalib_arena_init(arena, storage, capacity, max_ids) != 0;
    }

    inline void reset(arena_t *arena) {
        ::arenalib_arena_reset(arena);
    }

    inline arenalib_size_t available(const arena_t *arena) {
        return ::arenalib_arena_available(arena);
    }

    inline void *malloc(arena_t *arena, arenalib_size_t size) {
        return ::arenalib_arena_malloc(arena, size);
    }

    inline void *calloc(arena_t *arena, arenalib_size_t count, arenalib_size_t size) {
        return ::arenalib_arena_calloc(arena, count, size);
    }

    inline void free(arena_t *arena, void *ptr, arenalib_size_t size) {
        ::arenalib_arena_free(arena, ptr, size);
    }

    inline void *realloc(arena_t *arena, void *ptr, arenalib_size_t old_size, arenalib_size_t new_size) {
        return ::arenalib_arena_realloc(arena, ptr, old_size, new_size);
    }

    template <typename T>
    inline T *alloc(arena_t *arena, arenalib_size_t count = 1) {
        arenalib_size_t total;
        if (!::arenalib_size_mul(count, sizeof(T), &total)) return NULL;
        return reinterpret_cast<T *>(::arenalib_arena_malloc(arena, total));
    }

    template <typename T>
    inline T *alloc_zeroed(arena_t *arena, arenalib_size_t count = 1) {
        return reinterpret_cast<T *>(::arenalib_arena_calloc(arena, count, sizeof(T)));
    }

    template <typename T>
    inline T *resize(arena_t *arena, T *ptr, arenalib_size_t old_count, arenalib_size_t new_count) {
        arenalib_size_t old_size;
        arenalib_size_t new_size;
        if (!::arenalib_size_mul(old_count, sizeof(T), &old_size) ||
            !::arenalib_size_mul(new_count, sizeof(T), &new_size)) return NULL;
        return reinterpret_cast<T *>(::arenalib_arena_realloc(arena, ptr, old_size, new_size));
    }

    template <typename T>
    inline T *alloc_aligned(arena_t *arena, arenalib_size_t count = 1, arenalib_size_t alignment = default_alignment) {
        arenalib_size_t total;
        if (!::arenalib_size_mul(count, sizeof(T), &total)) return NULL;
        return reinterpret_cast<T *>(::arenalib_arena_malloc_align(arena, total, alignment));
    }

    inline void destroy(arena_t *arena) {
        ::arenalib_arena_destroy(arena);
    }

    inline void set_oom_callback(arena_t *arena, arenalib_oom_callback_t callback, void *user_data) {
        ::arenalib_arena_set_oom_callback(arena, callback, user_data);
    }

    inline void *malloc_align(arena_t *arena, arenalib_size_t size, arenalib_size_t alignment) {
        return ::arenalib_arena_malloc_align(arena, size, alignment);
    }

    inline marker_t get_marker(const arena_t *arena) {
        return ::arenalib_arena_get_marker(arena);
    }

    inline void release_marker(arena_t *arena, marker_t marker) {
        ::arenalib_arena_release_marker(arena, marker);
    }

    #if defined(__cpp_constexpr) || (defined(__cplusplus) && __cplusplus >= 201103L)
    static constexpr id_t invalid_id = {0xFFFFFFFF, 0xFFFF};
    #else
    static const id_t invalid_id = {0xFFFFFFFF, 0xFFFF};
    #endif
    
    inline id_t alloc_id(arena_t *arena, arenalib_size_t size) {
        return ::arenalib_arena_alloc_id(arena, size);
    }

    inline void *get_ptr(arena_t *arena, id_t id) {
        return ::arenalib_arena_get_ptr(arena, id);
    }

    inline void free_id(arena_t *arena, id_t id) {
        ::arenalib_arena_free_id(arena, id);
    }
}
#endif

#endif /* ARENALIB_H */
