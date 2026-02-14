#ifndef __ARENA_H
#define __ARENA_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#define PLAT_POSIX
#elif defined(_WIN32)
#define PLAT_WINDOWS
#else
#error "Unsupported Platform!"
#endif

#if defined(__clang__)
#define COMPILER_CLANG
#elif defined(__GNUC__)
#define COMPILER_GCC
#elif defined(_MSVC_VER)
#define COMPILER_MSVC
#else
#error "Unsupported Compiler!"
#endif

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#define THREAD_LOCAL __thread
#elif defined(COMPILER_MSVC)
#define THREAD_LOCAL __declspec(thread)
#else
#error "Unsupported Compiler!"
#endif

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#define ALIGN_OF(type) __alignof__(type)
#elif defined(COMPILER_MSVC)
#define ALIGN_OF(type) __alignof(type)
#else
#error "Unsupported Compiler!"
#endif

#ifndef PANIC
#define PANIC(msg, ...) \
	do { \
		fprintf(stderr, "Panic! %s:%d @%s\n", __FILE__, __LINE__, __func__); \
		fprintf(stderr, msg, ##__VA_ARGS__); \
		abort(); \
	} while (0) \

#endif

#define KiB(n) ((uint64_t)(n) << 10)
#define MiB(n) ((uint64_t)(n) << 20)
#define GiB(n) ((uint64_t)(n) << 30)

#define ALIGN_UP_POW2(size, align) (((size) + ((align) - 1)) & ~((align) - 1))
#define MIN(a, b) (a) < (b) ? (a) : (b)
#define MAX(a, b) (a) > (b) ? (a) : (b)

#ifndef ARENA_DEFAULT_ALIGNMENT
#define ARENA_DEFAULT_ALIGNMENT 8
#endif

#define ARENA_HEADER_SIZE_ALIGN (ALIGN_UP_POW2(sizeof(arena), ARENA_DEFAULT_ALIGNMENT))

#define ARENA_DEFAULT_RESERVE_SIZE MiB(1)
#define ARENA_DEFAULT_COMMIT_SIZE KiB(64)

uint32_t plat_get_pagesize(void);
void *plat_memory_reserve(size_t size);
int plat_memory_release(void *ptr, size_t size);
int plat_memory_commit(void *ptr, size_t size);
int plat_memory_decommit(void *ptr, size_t size);

typedef struct {
	size_t position;

	size_t committed_size;
	size_t commit_size;

	size_t reserve_size;
} arena;

arena *arena_new(size_t reserve_size, size_t commit_size);
inline void arena_release(arena *a) {
	plat_memory_release(a, a->reserve_size);
}

void *arena_alloc_align(arena *a, size_t size, size_t align);

inline void *arena_alloc(arena *a, size_t size) {
	void *out = arena_alloc_align(a, size, ARENA_DEFAULT_ALIGNMENT);
	if (out)
		memset(out, 0, size);

	return out;
}

inline void *arena_alloc_nz(arena *a, size_t size) {
	return arena_alloc_align(a, size, ARENA_DEFAULT_ALIGNMENT);
}

#define ALLOC_STRUCT(arena, T) \
	(T*)memset(arena_alloc_align(arena, sizeof(T), MAX(ARENA_DEFAULT_ALIGNMENT, ALIGN_OF(T))), 0, sizeof(T)) \

#define ALLOC_STRUCT_NZ(arena, T) \
	(T*)arena_alloc_align(arena, sizeof(T), MAX(ARENA_DEFAULT_ALIGNMENT, ALIGN_OF(T))) \

#define ALLOC_ARRAY(arena, T, count) \
	(T*)memset(arena_alloc_align(arena, sizeof(T) * count, MAX(ARENA_DEFAULT_ALIGNMENT, ALIGN_OF(T))), 0, sizeof(T) * count) \

#define ALLOC_ARRAY_NZ(arena, T, count) \
	(T*)arena_alloc_align(arena, sizeof(T) * count, MAX(ARENA_DEFAULT_ALIGNMENT, ALIGN_OF(T))) \

void arena_dealloc(arena *a, size_t size);
inline void arena_dealloc_to(arena *a, size_t position) {
	size_t size = position < a->position ? a->position - position : 0;
	arena_dealloc(a, size);
}

typedef struct {
	arena *arena;
	size_t position;
} arena_temp;

inline arena_temp arena_temp_begin(arena *a) {
	return (arena_temp) {
		.arena = a,
		.position = a->position
	};
}

inline void arena_temp_end(arena_temp temp) {
	arena_dealloc_to(temp.arena, temp.position);
}

void arena_scratch_alloc(void);
void arena_scratch_release(void);
arena_temp arena_scratch_begin(arena **conflicts, int conflict_count);
inline void arena_scratch_end(arena_temp scratch) {
	arena_temp_end(scratch);
}

inline void arena_scratch_init(void) {
	arena_scratch_alloc();
}

inline void arena_scratch_deinit(void) {
	arena_scratch_release();
}

#ifdef __cplusplus
}
#endif
#endif // __ARENA_H
