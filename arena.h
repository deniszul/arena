#ifndef __ARENA_H
#define __ARENA_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#define PLAT_POSIX
#elif defined(_WIN32)
#define PLAT_WINDOWS
#else
#error "Unsupported Platform!"
#endif

#define KiB(n) ((uint64_t)(n) << 10)
#define MiB(n) ((uint64_t)(n) << 20)
#define GiB(n) ((uint64_t)(n) << 30)

uint32_t plat_get_pagesize(void);
void *plat_memory_reserve(size_t size);
int plat_memory_release(void *ptr, size_t size);
int plat_memory_commit(void *ptr, size_t size);
int plat_memory_decommit(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif
#endif // __ARENA_H
