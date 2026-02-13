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

#ifdef __cplusplus
}
#endif
#endif // __ARENA_H
