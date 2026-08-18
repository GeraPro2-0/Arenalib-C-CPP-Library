# Arenalib

## arenalib.h — tiny single-header arena allocator with generational IDs

Usage:

- Include the header in your C or C++ project:

  #include "arenalib.h"

- Example: compile and run the test:

  - GCC:
    gcc -std=c11 -O2 arenalib_test_c.c -o test_c_arenalib ; ./test_c_arenalib
    g++ -std=c++11 -O2 arenalib_test_cpp.cpp -o test_cpp_arenalib ; ./test_cpp_arenalib
  ----------------------------------------------------------------------------
  - Clang:
    clang -std=c11 -O2 arenalib_test_c.c -o test_c_arenalib ; ./test_c_arenalib
    clang++ -std=c++11 -O2 arenalib_test_cpp.cpp -o test_cpp_arenalib ; ./test_cpp_arenalib
  ----------------------------------------------------------------------------
  - MSVC:
    cl /std:c11 /O2 arenalib_test_c.c /Fe:test_c_arenalib.exe && test_c_arenalib.exe
    cl /std:c++14 /O2 arenalib_test_cpp.cpp /Fe:test_cpp_arenalib.exe && test_cpp_arenalib.exe-

- Compatible versions:

  C: Compatible from C89/C90 onwards (safely degrades features like C11 _Atomic into standard types and loops when not supported).
  C++ (using ONLY C functions): Compatible from C++98 onwards.
  C++ (using C++'s own features): Compatible from C++11 onwards (C++14+ recommended for modern projects).

- Freestanding:

  Since this library does not depend on an operating system (there is no absolute dependency on a heavy standard runtime, making use of custom inline string operations), it can be used anywhere: in game engines, embedded drivers, custom kernels, or real-time bare-metal microchip applications.

## Features
- **Header-only**: Drop `arenalib.h` into your project source and you are ready to go.
- **Portability**: Supports classic legacy C standards (C89/C99 fallbacks), modern C11/C21 features, and full C++ namespace encapsulation.
- **Thread-Safe Global Pool**: Automatically scales by acquiring 1MB memory blocks via non-blocking atomic compare-and-swap (`__atomic_compare_exchange_n`) bitmaps when built with modern compilers.
- **Generational Secure IDs**: Eliminates dangling pointers completely by replacing raw pointers with static index tokens bound to strict temporal generation counters.
- **Deterministic Alignment**: Hand-rolled pointer math forces memory boundaries to stick to target alignment sizes (e.g., 16, 32, or 64-byte chunks for SIMD operations).

Notes:
- Defines core allocation macros and signatures:
  - `arenalib_arena_init`, `arenalib_arena_reset`, `arenalib_arena_destroy`
  - `arenalib_arena_malloc`, `arenalib_arena_calloc`, `arenalib_arena_realloc`, `arenalib_arena_free`
- Defines low-level utility operations for memory block handling:
  - `arenalib_align_up`, `arenalib_align_ptr`
  - `arenalib_memcpy`, `arenalib_memset`
- Advanced scoping and tracking mechanisms:
  - `arenalib_arena_get_marker`, `arenalib_arena_release_marker`
  - `arenalib_arena_alloc_id`, `arenalib_arena_get_ptr`, `arenalib_arena_free_id`
- Error resilience and callbacks:
  - Supports runtime execution hooks via `arenalib_arena_set_oom_callback` to catch Out-Of-Memory boundaries gracefully.
- Modern C++ type support:
  - Enclosed under `namespace arenalib` featuring custom types (`arena_t`, `marker_t`, `id_t`), clean overloaded syntax, and `constexpr` configurations for modern standards.
- Customizable static limits:
  - Tweak `#define ARENALIB_POOL_BLOCKS` or `#define ARENALIB_POOL_BLOCK_SIZE` before inclusion to perfectly mold the layout constraints to your system.