#include <stdio.h>
#include "arenalib.h"

/* 1. Out of Memory (OOM) callback function */
void my_oom_callback(void *user_data, arenalib_size_t size_requested)
{
    const char *system_name = (const char *)user_data;
    printf("[OOM ALERT] System '%s' ran out of memory while requesting %zu bytes.\n",
           system_name, size_requested);
}

int main(void)
{
    printf("--- Starting arenalib verification tests ---\n\n");

/* =========================================================================
 *  I. INITIALIZATION AND BASIC FUNCTIONS
 *  =========================================================================
 */

/* Allocate a static backing buffer to feed the arena */
#define INITIAL_CAPACITY 2048
    unsigned char backing_storage[INITIAL_CAPACITY];

    arenalib_arena_t arena;
    uint32_t max_ids = 5; /* Allow a maximum of 5 simultaneous IDs */

    /* Initialize the arena */
    if (!arenalib_arena_init(&arena, backing_storage, INITIAL_CAPACITY, max_ids))
    {
        printf("Error: Failed to initialize the arena.\n");
        return 1;
    }

    /* Set the Out Of Memory callback */
    arenalib_arena_set_oom_callback(&arena, my_oom_callback, (void *)"MainEngine");

    printf("Arena initialized. Available memory: %zu bytes.\n",
           arenalib_arena_available(&arena));

    /* =========================================================================
     *  II. MALLOC, CALLOC, FREE, AND REALLOC (Standard-style API)
     *  =========================================================================
     */

    /* arenalib_arena_malloc */
    int *number = (int *)arenalib_arena_malloc(&arena, sizeof(int));
    if (number)
    {
        *number = 42;
        printf("Allocated 'int' via malloc. Value: %d\n", *number);
    }

    /* arenalib_arena_calloc (Allocates and zeros out memory) */
    int *zero_array = (int *)arenalib_arena_calloc(&arena, 5, sizeof(int));
    printf("Allocated array via calloc: [%d, %d, %d, %d, %d]\n",
           zero_array[0], zero_array[1], zero_array[2], zero_array[3], zero_array[4]);

    /* arenalib_arena_realloc */
    /* Resize the single integer block to fit an array of 3 integers */
    int *realloc_array = (int *)arenalib_arena_realloc(&arena, number, sizeof(int), sizeof(int) * 3);
    if (realloc_array)
    {
        realloc_array[1] = 99;
        realloc_array[2] = 2026;
        printf("Realloc successful. New values: [%d, %d, %d]\n",
               realloc_array[0], realloc_array[1], realloc_array[2]);
    }

    /* arenalib_arena_free (Will only free if it was the last allocated element) */
    /* In this case, realloc_array is indeed the most recent allocation. */
    printf("Available before free: %zu bytes.\n", arenalib_arena_available(&arena));
    arenalib_arena_free(&arena, realloc_array, sizeof(int) * 3);
    printf("Available after free: %zu bytes.\n", arenalib_arena_available(&arena));

    /* =========================================================================
     *  III. MANUAL ALIGNMENT AND MARKERS
     *  =========================================================================
     */

    /* arenalib_arena_malloc_align (Forces an allocation aligned to 64 bytes) */
    double *aligned_data = (double *)arenalib_arena_malloc_align(&arena, sizeof(double) * 2, 64);
    printf("Pointer manually aligned to 64 bytes: %p\n", (void *)aligned_data);

    /* arenalib_arena_get_marker (Saves the current state of the arena) */
    arenalib_marker_t temporary_marker = arenalib_arena_get_marker(&arena);
    printf("Marker saved at used position: %zu\n", temporary_marker.used);

    /* Make temporary allocations that we will discard later */
    arenalib_arena_malloc(&arena, 200);
    arenalib_arena_malloc(&arena, 300);
    printf("Temporary memory used. Available: %zu bytes.\n", arenalib_arena_available(&arena));

    /* arenalib_arena_release_marker (Mass frees everything allocated after the marker) */
    arenalib_arena_release_marker(&arena, temporary_marker);
    printf("Marker released. Available restored to: %zu bytes.\n", arenalib_arena_available(&arena));

    /* =========================================================================
     *  IV. SECURE ID SYSTEM (GENERATIONAL)
     *  =========================================================================
     */
    printf("\n--- Testing Generational ID System ---\n");

    /* arenalib_arena_alloc_id */
    arenalib_id_t entity_id = arenalib_arena_alloc_id(&arena, 100);
    printf("ID Allocated -> Index: %u, Generation: %u\n", entity_id.index, entity_id.generation);

    /* arenalib_arena_get_ptr (Retrieves the raw pointer using the ID token) */
    char *entity_data = (char *)arenalib_arena_get_ptr(&arena, entity_id);
    if (entity_data)
    {
        arenalib_memset(entity_data, 'A', 99); /* Testing inline arenalib_memset */
        entity_data[99] = '\0';
        printf("First character of ID data payload: %c\n", entity_data[0]);
    }

    /* arenalib_arena_free_id (Invalidates the ID by incrementing its generation) */
    arenalib_arena_free_id(&arena, entity_id);
    printf("ID Freed.\n");

    /* Try to access it again using the same ID token (Must fail and return NULL) */
    char *dangling_data = (char *)arenalib_arena_get_ptr(&arena, entity_id);
    if (dangling_data == NULL)
    {
        printf("Success: Access denied. The ID is now invalid (Dangling pointer prevented).\n");
    }

    arena.generations[entity_id.index] = 0xFFFFFFFFu;
    {
        arenalib_id_t wide_generation_id = arenalib_arena_alloc_id(&arena, 16);
        if (wide_generation_id.generation != 0xFFFFFFFFu)
        {
            printf("Error: Failed to preserve the 32-bit ID generation.\n");
            return 1;
        }
        arenalib_arena_free_id(&arena, wide_generation_id);
        wide_generation_id = arenalib_arena_alloc_id(&arena, 32);
        if (wide_generation_id.generation != 0u)
        {
            printf("Error: ID generation wrapped at 32 bits.\n");
            return 1;
        }
        arenalib_arena_free_id(&arena, wide_generation_id);
    }

    {
        arenalib_id_t id_before_marker = arenalib_arena_alloc_id(&arena, 24);
        arenalib_marker_t id_marker = arenalib_arena_get_marker(&arena);
        arenalib_id_t id_after_marker = arenalib_arena_alloc_id(&arena, 24);
        arenalib_arena_release_marker(&arena, id_marker);
        if (arenalib_arena_get_ptr(&arena, id_before_marker) == NULL ||
            arenalib_arena_get_ptr(&arena, id_after_marker) != NULL)
        {
            printf("Error: Marker rollback did not preserve/invalidate the expected IDs.\n");
            return 1;
        }
        arenalib_arena_free_id(&arena, id_before_marker);
    }

    /* Verification against the default invalid ID macro */
    arenalib_id_t invalid_id_check = ARENALIB_INVALID_ID;
    if (entity_id.index != invalid_id_check.index)
    {
        printf("The recycled ID slot retains its original index but changes generation.\n");
    }

    /* =========================================================================
     *  V. AUTOMATIC GROWTH VIA THE GLOBAL STATIC POOL
     *  =========================================================================
     */
    printf("\n--- Testing Arena Growth (Static Pool) ---\n");

    /* Request a massive chunk of memory to exceed the initial 2048 bytes capacity. */
    /* The arena will automatically call `arenalib_pool_acquire_block` under the hood. */
    printf("Requesting 10,000 bytes (Exceeds original backing storage size)...\n");
    void *large_block = arenalib_arena_malloc(&arena, 10000);

    if (large_block != NULL)
    {
        printf("Success! The arena automatically linked a new block from the static pool.\n");
    }

    /* Actively trigger an OOM condition by asking for a size that exceeds pool limits */
    printf("Requesting an impossible size to force the OOM callback to fire...\n");
    void *impossible_block = arenalib_arena_malloc(&arena, ARENALIB_POOL_BLOCK_SIZE * 2);
    (void)impossible_block; /* Suppress unused variable warning */

    printf("\n--- Stress test: repeated allocation/reset under load ---\n");
    {
        #define STRESS_CAPACITY 16384
        unsigned char stress_storage[STRESS_CAPACITY];
        arenalib_arena_t stress_arena;
        int i;
        int j;

        if (!arenalib_arena_init(&stress_arena, stress_storage, STRESS_CAPACITY, 0))
        {
            printf("Error: Failed to initialize the stress arena.\n");
            return 1;
        }

        for (i = 0; i < 200; ++i)
        {
            void *blocks[32];
            for (j = 0; j < 32; ++j)
            {
                arenalib_size_t block_size = (arenalib_size_t)(8 + (((i * 17) + (j * 13)) % 256));
                blocks[j] = arenalib_arena_malloc(&stress_arena, block_size);
                if (!blocks[j])
                {
                    printf("Stress test failed at iteration %d, block %d.\n", i, j);
                    arenalib_arena_destroy(&stress_arena);
                    return 1;
                }
                arenalib_memset(blocks[j], (i + j) & 0xFF, block_size);
            }

            if (!arenalib_arena_malloc(&stress_arena, 600000) ||
                !arenalib_arena_malloc(&stress_arena, 600000))
            {
                printf("Stress test failed to acquire a pool block at iteration %d.\n", i);
                arenalib_arena_destroy(&stress_arena);
                return 1;
            }
            arenalib_arena_reset(&stress_arena);
        }

        printf("Stress test passed: 200 allocation/reset rounds completed.\n");
        arenalib_arena_destroy(&stress_arena);
    }

    /* =========================================================================
     *  VI. CLEANUP AND DESTRUCTION
     *  =========================================================================
     */

    /* arenalib_arena_reset (Frees pool blocks and sets usage tracking counters back to 0) */
    {
        unsigned char reset_storage[256];
        arenalib_arena_t reset_arena;
        arenalib_id_t id_before_reset;
        if (!arenalib_arena_init(&reset_arena, reset_storage, sizeof(reset_storage), 2))
        {
            printf("Error: Failed to initialize the reset test arena.\n");
            return 1;
        }
        id_before_reset = arenalib_arena_alloc_id(&reset_arena, 16);
        if (id_before_reset.index == 0xFFFFFFFFu)
        {
            printf("Error: Failed to allocate the reset test ID.\n");
            arenalib_arena_destroy(&reset_arena);
            return 1;
        }
        arenalib_arena_reset(&reset_arena);
        if (arenalib_arena_get_ptr(&reset_arena, id_before_reset) != NULL)
        {
            printf("Error: Arena reset did not invalidate an active ID.\n");
            arenalib_arena_destroy(&reset_arena);
            return 1;
        }
        arenalib_arena_destroy(&reset_arena);
    }
    printf("\nArena reset complete. Backing capacity restored to baseline.\n");

    /* arenalib_arena_destroy (Clears structural data references completely) */
    arenalib_arena_destroy(&arena);
    printf("Arena destroyed successfully.\n");

    return 0;
}