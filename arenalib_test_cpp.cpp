#include <iostream>
#include <iomanip>
#include "arenalib.h"
#if ARENALIB_HAS_THREAD_LOCKS
#include <atomic>
#include <thread>
#endif

// 1. Out of Memory (OOM) callback function
void my_oom_callback(void *user_data, arenalib_size_t size_requested)
{
    const char *system_name = static_cast<const char *>(user_data);
    std::cout << "[OOM ALERT] System '" << system_name
              << "' ran out of memory while requesting " << size_requested << " bytes.\n";
}

struct reentrant_oom_context
{
    arenalib::arena_t *arena;
    arenalib_size_t available;
    bool called;
};

static void reentrant_oom_callback(void *user_data, arenalib_size_t)
{
    reentrant_oom_context *context = static_cast<reentrant_oom_context *>(user_data);
    context->available = arenalib::available(context->arena);
    context->called = true;
}

int main()
{
    std::cout << "--- Starting arenalib C++ verification tests ---\n\n";

    // =========================================================================
    // I. INITIALIZATION AND BASIC FUNCTIONS (C++ Interface)
    // =========================================================================

    constexpr arenalib_size_t INITIAL_CAPACITY = 2048;
    unsigned char backing_storage[INITIAL_CAPACITY];

    // We use the type alias provided by the C++ namespace
    arenalib::arena_t arena;
    uint32_t max_ids = 5; // Maximum of 5 simultaneous IDs

    // Initialize the arena using arenalib::init (returns a native C++ bool)
    if (!arenalib::init(&arena, backing_storage, INITIAL_CAPACITY, max_ids))
    {
        std::cerr << "Error: Failed to initialize the arena.\n";
        return 1;
    }

    // Configure the Out of Memory callback
    arenalib::set_oom_callback(&arena, my_oom_callback, (void *)"GraphicsEngine");

    std::cout << "Arena initialized. Default alignment constant: " << arenalib::default_alignment << " bytes.\n";
    std::cout << "Available memory: " << arenalib::available(&arena) << " bytes.\n";

    // =========================================================================
    // II. MALLOC, CALLOC, FREE, AND REALLOC
    // =========================================================================

    // arenalib::malloc
    int *number = static_cast<int *>(arenalib::malloc(&arena, sizeof(int)));
    if (number)
    {
        *number = 1337;
        std::cout << "Allocated 'int' via arenalib::malloc. Value: " << *number << "\n";
    }

    // arenalib::calloc (Allocates and zeros out memory)
    int *zero_array = static_cast<int *>(arenalib::calloc(&arena, 5, sizeof(int)));
    std::cout << "Allocated array via arenalib::calloc: [";
    for (int i = 0; i < 5; ++i)
    {
        std::cout << zero_array[i] << (i < 4 ? ", " : "");
    }
    std::cout << "]\n";

    // arenalib::realloc
    // Resize the individual integer block to fit an array of 3 integers
    int *realloc_array = static_cast<int *>(arenalib::realloc(&arena, number, sizeof(int), sizeof(int) * 3));
    if (realloc_array)
    {
        realloc_array[1] = 888;
        realloc_array[2] = 2026;
        std::cout << "Realloc successful. New values: ["
                  << realloc_array[0] << ", " << realloc_array[1] << ", " << realloc_array[2] << "]\n";
    }

    // arenalib::free (Only frees if it was the last allocated element in the arena)
    std::cout << "Available before free: " << arenalib::available(&arena) << " bytes.\n";
    arenalib::free(&arena, realloc_array, sizeof(int) * 3);
    std::cout << "Available after free: " << arenalib::available(&arena) << " bytes.\n";

    // =========================================================================
    // III. MANUAL ALIGNMENT AND MARKERS
    // =========================================================================

    // arenalib::malloc_align (Forces an allocation aligned to 64 bytes)
    double *aligned_data = static_cast<double *>(arenalib::malloc_align(&arena, sizeof(double) * 2, 64));
    std::cout << "Pointer manually aligned to 64 bytes: " << static_cast<void *>(aligned_data) << "\n";

    // arenalib::get_marker (Saves the current state of the arena)
    arenalib::marker_t temporary_marker = arenalib::get_marker(&arena);
    std::cout << "Marker saved at position: " << temporary_marker.used << "\n";

    // Make temporary allocations that we will discard later
    arenalib::malloc(&arena, 150);
    arenalib::malloc(&arena, 250);
    std::cout << "Temporary memory used. Available: " << arenalib::available(&arena) << " bytes.\n";

    // arenalib::release_marker (Mass frees everything allocated after the marker)
    arenalib::release_marker(&arena, temporary_marker);
    std::cout << "Marker released. Available restored to: " << arenalib::available(&arena) << " bytes.\n";

    // =========================================================================
    // IV. SECURE ID SYSTEM (GENERATIONAL)
    // =========================================================================
    std::cout << "\n--- Testing Generational ID System in C++ ---\n";

    // arenalib::alloc_id
    arenalib::id_t entity_id = arenalib::alloc_id(&arena, 100);
    std::cout << "ID Allocated -> Index: " << entity_id.index << ", Generation: " << entity_id.generation << "\n";

    // arenalib::get_ptr (Retrieves the raw pointer using the ID token)
    char *entity_data = static_cast<char *>(arenalib::get_ptr(&arena, entity_id));
    if (entity_data)
    {
        // Since we're in C++, we can use std::fill, but we'll use native functions if desired.
        entity_data[0] = 'C';
        entity_data[1] = '+';
        entity_data[2] = '+';
        entity_data[3] = '\0';
        std::cout << "String stored inside ID payload: " << entity_data << "\n";
    }

    // arenalib::free_id (Invalidates the ID by incrementing its generation)
    arenalib::free_id(&arena, entity_id);
    std::cout << "ID Freed.\n";

    // Try to access it again using the same ID token (Must fail and return NULL)
    char *dangling_data = static_cast<char *>(arenalib::get_ptr(&arena, entity_id));
    if (dangling_data == nullptr)
    {
        std::cout << "Success: Access denied. The ID is now invalid (Dangling pointer prevented).\n";
    }

    arena.generations[entity_id.index] = 0xFFFFFFFFu;
    arenalib::id_t wide_generation_id = arenalib::alloc_id(&arena, 32);
    if (wide_generation_id.generation != 0xFFFFFFFFu)
    {
        std::cerr << "Error: Failed to preserve the 32-bit ID generation.\n";
        return 1;
    }
    arenalib::free_id(&arena, wide_generation_id);
    wide_generation_id = arenalib::alloc_id(&arena, 32);
    if (wide_generation_id.generation != 0u)
    {
        std::cerr << "Error: ID generation wrapped at 32 bits.\n";
        return 1;
    }
    arenalib::free_id(&arena, wide_generation_id);

    {
        arenalib::id_t id_before_marker = arenalib::alloc_id(&arena, 24);
        arenalib::marker_t id_marker = arenalib::get_marker(&arena);
        arenalib::id_t id_after_marker = arenalib::alloc_id(&arena, 24);
        arenalib::release_marker(&arena, id_marker);
        if (arenalib::get_ptr(&arena, id_before_marker) == nullptr ||
            arenalib::get_ptr(&arena, id_after_marker) != nullptr)
        {
            std::cerr << "Error: Marker rollback did not preserve/invalidate the expected IDs.\n";
            return 1;
        }
        arenalib::free_id(&arena, id_before_marker);
    }

    // Checking against the C++ constant 'invalid_id'
    if (entity_id.index != arenalib::invalid_id.index)
    {
        std::cout << "The recycled ID slot retains its index but updated its generation token.\n";
    }

    // =========================================================================
    // V. AUTOMATIC GROWTH VIA THE GLOBAL STATIC POOL
    // =========================================================================
    std::cout << "\n--- Testing Arena Growth (Static Pool) ---\n";

    // Request a massive chunk of memory to exceed the initial 2048 bytes capacity.
    // The arena will automatically call `arenalib::malloc` under the hood.
    std::cout << "Requesting 15,000 bytes (Exceeds original backing storage size)...\n";
    void *large_block = arenalib::malloc(&arena, 15000);

    if (large_block != nullptr)
    {
        std::cout << "Success! The arena automatically linked a new block from the static pool.\n";
    }

    // We trigger a real OOM condition by requesting a size that exceeds the pool's per-block limit
    std::cout << "Requesting an impossible size to force the OOM callback to fire...\n";
    void *impossible_block = arenalib::malloc(&arena, ARENALIB_POOL_BLOCK_SIZE * 2);
    (void)impossible_block; // Suprimir advertencia de variable sin usar

#if ARENALIB_HAS_THREAD_LOCKS
    std::cout << "\n--- Thread-safety test: shared arena and shared pool ---\n";
    {
        constexpr int THREAD_COUNT = 4;
        constexpr int ALLOCATIONS_PER_THREAD = 256;
        constexpr arenalib_size_t ALLOCATION_SIZE = 64;
        unsigned char shared_storage[THREAD_COUNT * ALLOCATIONS_PER_THREAD * ALLOCATION_SIZE + 64];
        arenalib::arena_t shared_arena;
        void *shared_blocks[THREAD_COUNT][ALLOCATIONS_PER_THREAD] = {};
        int shared_failures[THREAD_COUNT] = {};
        std::thread shared_workers[THREAD_COUNT];

        if (!arenalib::init(&shared_arena, shared_storage, sizeof(shared_storage)))
        {
            std::cerr << "Error: Failed to initialize the shared arena.\n";
            return 1;
        }

        for (int thread_index = 0; thread_index < THREAD_COUNT; ++thread_index)
        {
            shared_workers[thread_index] = std::thread([&, thread_index]() {
                for (int allocation_index = 0; allocation_index < ALLOCATIONS_PER_THREAD; ++allocation_index)
                {
                    unsigned char *block = static_cast<unsigned char *>(arenalib::malloc(&shared_arena, ALLOCATION_SIZE));
                    if (block == nullptr)
                    {
                        shared_failures[thread_index] = 1;
                        return;
                    }
                    shared_blocks[thread_index][allocation_index] = block;
                    arenalib_memset(block, thread_index + 1, ALLOCATION_SIZE);
                }
            });
        }
        for (int thread_index = 0; thread_index < THREAD_COUNT; ++thread_index)
        {
            shared_workers[thread_index].join();
        }
        for (int thread_index = 0; thread_index < THREAD_COUNT; ++thread_index)
        {
            for (int allocation_index = 0; allocation_index < ALLOCATIONS_PER_THREAD; ++allocation_index)
            {
                const unsigned char *block = static_cast<const unsigned char *>(shared_blocks[thread_index][allocation_index]);
                if (shared_failures[thread_index] || block == nullptr)
                {
                    std::cerr << "Error: Concurrent shared-arena allocation failed.\n";
                    arenalib::destroy(&shared_arena);
                    return 1;
                }
                for (arenalib_size_t byte_index = 0; byte_index < ALLOCATION_SIZE; ++byte_index)
                {
                    if (block[byte_index] != thread_index + 1)
                    {
                        std::cerr << "Error: Concurrent allocations overlapped or were corrupted.\n";
                        arenalib::destroy(&shared_arena);
                        return 1;
                    }
                }
            }
        }
        arenalib::destroy(&shared_arena);

        unsigned char pool_storage[THREAD_COUNT][256];
        arenalib::arena_t pool_arenas[THREAD_COUNT];
        void *pool_blocks[THREAD_COUNT] = {};
        int pool_failures[THREAD_COUNT] = {};
        std::atomic<int> pool_ready(0);
        std::atomic<int> pool_checked(0);
        std::thread pool_workers[THREAD_COUNT];
        for (int thread_index = 0; thread_index < THREAD_COUNT; ++thread_index)
        {
            if (!arenalib::init(&pool_arenas[thread_index], pool_storage[thread_index], sizeof(pool_storage[thread_index])))
            {
                std::cerr << "Error: Failed to initialize a pool-test arena.\n";
                return 1;
            }
        }
        for (int thread_index = 0; thread_index < THREAD_COUNT; ++thread_index)
        {
            pool_workers[thread_index] = std::thread([&, thread_index]() {
                void *block = arenalib::malloc(&pool_arenas[thread_index], 4096);
                pool_blocks[thread_index] = block;
                if (block == nullptr)
                {
                    pool_failures[thread_index] = 1;
                }
                else
                {
                    arenalib_memset(block, thread_index + 1, 4096);
                }
                pool_ready.fetch_add(1);
                while (pool_ready.load() != THREAD_COUNT)
                {
                }
                for (int other_index = 0; other_index < THREAD_COUNT; ++other_index)
                {
                    if (other_index != thread_index && block == pool_blocks[other_index])
                    {
                        pool_failures[thread_index] = 1;
                    }
                }
                if (block != nullptr)
                {
                    const unsigned char *bytes = static_cast<const unsigned char *>(block);
                    for (int byte_index = 0; byte_index < 4096; ++byte_index)
                    {
                        if (bytes[byte_index] != thread_index + 1)
                        {
                            pool_failures[thread_index] = 1;
                            break;
                        }
                    }
                }
                pool_checked.fetch_add(1);
                while (pool_checked.load() != THREAD_COUNT)
                {
                }
                arenalib::reset(&pool_arenas[thread_index]);
            });
        }
        for (int thread_index = 0; thread_index < THREAD_COUNT; ++thread_index)
        {
            pool_workers[thread_index].join();
            if (pool_failures[thread_index])
            {
                std::cerr << "Error: Concurrent pool acquisition failed.\n";
                for (int cleanup_index = 0; cleanup_index < THREAD_COUNT; ++cleanup_index)
                {
                    arenalib::destroy(&pool_arenas[cleanup_index]);
                }
                return 1;
            }
            arenalib::destroy(&pool_arenas[thread_index]);
        }
        std::cout << "Thread-safety test passed.\n";
    }

    {
        unsigned char callback_storage[256];
        arenalib::arena_t callback_arena;
        reentrant_oom_context callback_context = {&callback_arena, 0, false};
        if (!arenalib::init(&callback_arena, callback_storage, sizeof(callback_storage)))
        {
            std::cerr << "Error: Failed to initialize the callback test arena.\n";
            return 1;
        }
        arenalib::set_oom_callback(&callback_arena, reentrant_oom_callback, &callback_context);
        if (arenalib::malloc(&callback_arena, ARENALIB_POOL_BLOCK_SIZE - sizeof(arenalib_arena_t) + 1) != nullptr ||
            !callback_context.called || callback_context.available == 0)
        {
            std::cerr << "Error: OOM callback could not safely re-enter the arena API.\n";
            arenalib::destroy(&callback_arena);
            return 1;
        }
        arenalib::destroy(&callback_arena);
    }
#endif

    // =========================================================================
    // VI. CLEANUP AND DESTRUCTION
    // =========================================================================

    // arenalib::reset (Frees pool blocks and sets usage tracking counters back to 0)
    {
        unsigned char reset_storage[256];
        arenalib::arena_t reset_arena;
        if (!arenalib::init(&reset_arena, reset_storage, sizeof(reset_storage), 2))
        {
            std::cerr << "Error: Failed to initialize the reset test arena.\n";
            return 1;
        }
        arenalib::id_t const id_before_reset = arenalib::alloc_id(&reset_arena, 16);
        if (id_before_reset.index == 0xFFFFFFFFu)
        {
            std::cerr << "Error: Failed to allocate the reset test ID.\n";
            arenalib::destroy(&reset_arena);
            return 1;
        }
        arenalib::reset(&reset_arena);
        if (arenalib::get_ptr(&reset_arena, id_before_reset) != nullptr)
        {
            std::cerr << "Error: Arena reset did not invalidate an active ID.\n";
            arenalib::destroy(&reset_arena);
            return 1;
        }
        arenalib::destroy(&reset_arena);
    }
    std::cout << "\nArena reset complete. Backing capacity restored to baseline.\n";

    // arenalib::destroy (Clears structural data references completely)
    arenalib::destroy(&arena);
    std::cout << "Arena destroyed successfully.\n";

    return 0;
}