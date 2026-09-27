# Arenalib

## arenalib.h — tiny single-header arena allocator with generational IDs

Usage:

- Include the header in your C or C++ project:

  #include "arenalib.h"

- Example: compile and run the test:

  - GCC:
    gcc -std=c11 -O2 arenalib_test_c.c -o test_c_arenalib ; ./test_c_arenalib
    g++ -std=c++11 -pthread -O2 arenalib_test_cpp.cpp -o test_cpp_arenalib ; ./test_cpp_arenalib
  ----------------------------------------------------------------------------
  - Clang:
    clang -std=c11 -O2 arenalib_test_c.c -o test_c_arenalib ; ./test_c_arenalib
    clang++ -std=c++11 -pthread -O2 arenalib_test_cpp.cpp -o test_cpp_arenalib ; ./test_cpp_arenalib
  ----------------------------------------------------------------------------
  - MSVC:
    cl /std:c11 /O2 arenalib_test_c.c /Fe:test_c_arenalib.exe && test_c_arenalib.exe
    cl /std:c++14 /O2 arenalib_test_cpp.cpp /Fe:test_cpp_arenalib.exe && test_cpp_arenalib.exe

- Compatible versions:

  C: Compatible from C89/C90 onwards.
  C++ (using ONLY C functions): Compatible from C++98 onwards.
  C++ (using C++'s own features): Compatible from C++11 onwards (C++14+ recommended for modern projects).

- Freestanding:

  Since this library does not depend on an operating system (there is no absolute dependency on a heavy standard runtime, making use of custom inline string operations), it can be used anywhere: in game engines, embedded drivers, custom kernels, or real-time bare-metal microchip applications.

## Features
- **Header-only**: Drop `arenalib.h` into your project source and you are ready to go.
- **Portability**: Supports classic legacy C standards (C89/C99 fallbacks), modern C11/C21 features, and full C++ namespace encapsulation.
- **Thread-safe allocation API**: Serializes operations on each arena and protects the shared pool with a constant-time free-index stack on supported compilers.
- **Generational Secure IDs**: Eliminates dangling pointers completely by replacing raw pointers with static index tokens bound to strict temporal generation counters.
- **Deterministic Alignment**: Hand-rolled pointer math forces memory boundaries to stick to target alignment sizes (e.g., 16, 32, or 64-byte chunks for SIMD operations).

## Allocation contract

- Allocation bookkeeping uses the active tail block, and the pool uses an O(1) free-index stack; neither path scans the block chain or bitmap.
- `free` reclaims memory only when the pointer and size describe the most recent allocation in the active tail block. Other frees are ignored.
- `realloc` resizes that tail allocation in place when possible; otherwise it allocates and copies, so the copy cost depends on the requested data size.
- Markers can be released only while their block is still the active tail. Marker rollback visits only active IDs allocated after that marker.
- `reset` and `destroy` return owned pool blocks with a constant-time list splice. `reset` also invalidates each active ID, so that part costs O(number of active IDs), not O(`max_ids`).
- Arena initialization builds the initial ID free list and is O(`max_ids`); it is setup work, not an allocation hot path.
- ID indices and generations are both 32-bit values.

## Thread safety

- On GCC, Clang, and MSVC, calls to the arena API may run concurrently on the same arena. Calls on different arenas may also run concurrently; access to the global pool is synchronized. The locks are spinlocks and may busy-wait under contention.
- For a multi-translation-unit program, define `ARENALIB_IMPLEMENTATION` before including `arenalib.h` in exactly one source file. Define `ARENALIB_USE_EXTERNAL_POOL` before including it in the other source files that share arenas. Without these macros, each translation unit has an independent static pool.
- Other compilers can provide `ARENALIB_LOCK_TYPE`, `ARENALIB_LOCK_INITIALIZER`, `ARENALIB_LOCK_INIT`, `ARENALIB_LOCK_ACQUIRE`, and `ARENALIB_LOCK_RELEASE`. If the compiler has no built-in lock support and these hooks are not supplied, `ARENALIB_HAS_THREAD_LOCKS` is `0` and callers must synchronize externally.
- Initialize an arena before sharing it between threads, and stop/join users before destroying it. The lock protects allocator metadata, not reads/writes through returned pointers; callers must synchronize the lifetime and contents of allocated memory themselves.
- OOM callbacks run after the arena lock is released. Synchronize callback-owned state if multiple arenas can invoke the same callback concurrently.

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