#include "arena.h"

#include <assert.h>

#if defined(PLAT_POSIX)
#include <unistd.h>
#include <sys/mman.h>
#include <bits/sysconf.h>

uint32_t plat_get_pagesize(void) {
	static uint32_t pagesize = 0;
	static int is_reserved = 0;
	if (!is_reserved) {
		pagesize = getpagesize();
		is_reserved = 1;
	}

	return pagesize;
}

void *plat_memory_reserve(size_t size) {
	void *result = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == MAP_FAILED)
		result = NULL;

	return result;
}

int plat_memory_release(void *ptr, size_t size) {
	return munmap(ptr, size) != -1;
}

int plat_memory_commit(void *ptr, size_t size) {
	return mprotect(ptr, size, PROT_READ | PROT_WRITE) != -1;
}

int plat_memory_decommit(void *ptr, size_t size) {
	if (madvise(ptr, size, MADV_DONTNEED) < 0)
		return 0;
	if (mprotect(ptr, size, PROT_NONE) < 0)
		return 0;
	return 1;
}
#elif defined(PLAT_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

uint32_t plat_get_pagesize(void) {
	static uint32_t pagesize = 0;
	static int is_reserved = 0;
	if (!is_reserved) {
		SYSTEM_INFO sys_info = { 0 };
		GetSystemInfo(&info);
		pagesize = sys_info.dwPageSize;
		is_reserved = 1;
	}

	return pagesize;
}

void *plat_memory_reserve(size_t size) {
	return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
}

int plat_memory_release(void *ptr, size_t size) {
	return VirtualFree(ptr, 0, MEM_RELEASE) != 0;
}

int plat_memory_commit(void *ptr, size_t size) {
	return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

int plat_memory_decommit(void *ptr, size_t size) {
	return VirtualFree(ptr, size, MEM_DECOMMIT) != 0;
}
#else
#error "Unsupported Platform!"
#endif

arena *arena_new(size_t reserve_size, size_t commit_size) {
	if (reserve_size < 1)
		reserve_size = ARENA_DEFAULT_RESERVE_SIZE;
	if (commit_size < 1)
		commit_size = ARENA_DEFAULT_COMMIT_SIZE;

	reserve_size = reserve_size + ARENA_HEADER_SIZE_ALIGN;
	commit_size = MAX(commit_size, ARENA_HEADER_SIZE_ALIGN);

	uint32_t pagesize = plat_get_pagesize();
	reserve_size = ALIGN_UP_POW2(reserve_size, pagesize);
	commit_size = ALIGN_UP_POW2(commit_size, pagesize);

	void *base = plat_memory_reserve(reserve_size);
	if (!plat_memory_commit(base, commit_size))
		PANIC("Commit new arena failed\n");

	arena *a = (arena*)base;
	a->position = ARENA_HEADER_SIZE_ALIGN;
	a->committed_size = commit_size;
	a->commit_size = commit_size;
	a->reserve_size = reserve_size;

	return a;
}

void *arena_alloc_align(arena *a, size_t size, size_t align) {
	assert(a);

	if (size == 0)
		return NULL;

	assert((align & (align - 1)) == 0);
	size_t pos_aligned = ALIGN_UP_POW2(a->position, align);

	size_t new_pos = pos_aligned + size;
	if (new_pos > a->reserve_size)
		return NULL;

	if (new_pos > a->committed_size) {
		size_t new_committed_size = ALIGN_UP_POW2(new_pos, a->commit_size);
		new_committed_size = MIN(new_committed_size, a->reserve_size);

		uint8_t *commit_ptr = (uint8_t*)a + a->committed_size;
		size_t commit_size = new_committed_size - a->committed_size;
		if (!plat_memory_commit(commit_ptr, commit_size))
			return NULL;

		a->committed_size = new_committed_size;
	}

	a->position = new_pos;
	uint8_t *out = (uint8_t*)a + pos_aligned;

	return (void*)out;
}
