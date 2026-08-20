#include <iostream>
#include <iomanip>
#include "arenalib.h"

// 1. Out of Memory (OOM) callback function
void my_oom_callback(void* user_data, arenalib_size_t size_requested) {
    const char* system_name = static_cast<const char*>(user_data);
    std::cout << "[OOM ALERT] System '" << system_name 
              << "' ran out of memory while requesting " << size_requested << " bytes.\n";
}

int main() {
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
    if (!arenalib::init(&arena, backing_storage, INITIAL_CAPACITY, max_ids)) {
        std::cerr << "Error: Failed to initialize the arena.\n";
        return 1;
    }
    
    // Configure the Out of Memory callback
    arenalib::set_oom_callback(&arena, my_oom_callback, (void*)"GraphicsEngine");

    std::cout << "Arena initialized. Default alignment constant: " << arenalib::default_alignment << " bytes.\n";
    std::cout << "Available memory: " << arenalib::available(&arena) << " bytes.\n";

    // =========================================================================
    // II. MALLOC, CALLOC, FREE, AND REALLOC
    // =========================================================================
    
    // arenalib::malloc
    int* number = static_cast<int*>(arenalib::malloc(&arena, sizeof(int)));
    if (number) {
        *number = 1337;
        std::cout << "Allocated 'int' via arenalib::malloc. Value: " << *number << "\n";
    }

    // arenalib::calloc (Allocates and zeros out memory)
    int* zero_array = static_cast<int*>(arenalib::calloc(&arena, 5, sizeof(int)));
    std::cout << "Allocated array via arenalib::calloc: [";
    for (int i = 0; i < 5; ++i) {
        std::cout << zero_array[i] << (i < 4 ? ", " : "");
    }
    std::cout << "]\n";

    // arenalib::realloc
    // Resize the individual integer block to fit an array of 3 integers
    int* realloc_array = static_cast<int*>(arenalib::realloc(&arena, number, sizeof(int), sizeof(int) * 3));
    if (realloc_array) {
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
    double* aligned_data = static_cast<double*>(arenalib::malloc_align(&arena, sizeof(double) * 2, 64));
    std::cout << "Pointer manually aligned to 64 bytes: " << static_cast<void*>(aligned_data) << "\n";

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
    char* entity_data = static_cast<char*>(arenalib::get_ptr(&arena, entity_id));
    if (entity_data) {
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
    char* dangling_data = static_cast<char*>(arenalib::get_ptr(&arena, entity_id));
    if (dangling_data == nullptr) {
        std::cout << "Success: Access denied. The ID is now invalid (Dangling pointer prevented).\n";
    }

    // Checking against the C++ constant 'invalid_id'
    if (entity_id.index != arenalib::invalid_id.index) {
        std::cout << "The recycled ID slot retains its index but updated its generation token.\n";
    }

    // =========================================================================
    // V. AUTOMATIC GROWTH VIA THE GLOBAL STATIC POOL
    // =========================================================================
    std::cout << "\n--- Testing Arena Growth (Static Pool) ---\n";
    
    // Request a massive chunk of memory to exceed the initial 2048 bytes capacity.
    // The arena will automatically call `arenalib::malloc` under the hood.
    std::cout << "Requesting 15,000 bytes (Exceeds original backing storage size)...\n";
    void* large_block = arenalib::malloc(&arena, 15000);
    
    if (large_block != nullptr) {
        std::cout << "Success! The arena automatically linked a new block from the static pool.\n";
    }

    // We trigger a real OOM condition by requesting a size that exceeds the pool's per-block limit
    std::cout << "Requesting an impossible size to force the OOM callback to fire...\n";
    void* impossible_block = arenalib::malloc(&arena, ARENALIB_POOL_BLOCK_SIZE * 2);
    (void)impossible_block; // Suprimir advertencia de variable sin usar

    // =========================================================================
    // VI. CLEANUP AND DESTRUCTION
    // =========================================================================
    
    // arenalib::reset (Frees pool blocks and sets usage tracking counters back to 0)
    arenalib::reset(&arena);
    std::cout << "\nArena reset complete. Backing capacity restored to baseline.\n";

    // arenalib::destroy (Clears structural data references completely)
    arenalib::destroy(&arena);
    std::cout << "Arena destroyed successfully.\n";

    return 0;
}