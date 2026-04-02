#include "arena.h"

#include <stdio.h>
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

#define ARENA_HEADER_SIZE_ALIGN (ALIGN_UP_POW2(sizeof(arena), ARENA_DEFAULT_ALIGNMENT))

#define ALIGN_UP_POW2(size, align) (((size) + ((align) - 1)) & ~((align) - 1))

#ifndef PANIC
#define PANIC(msg) \
	do { \
		fprintf(stderr, "Panic! %s:%d @%s\n", __FILE__, __LINE__, __func__); \
		fprintf(stderr, msg); \
		abort(); \
	} while (0)
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
        return NULL;

	arena *a = (arena*)base;
	a->position = ARENA_HEADER_SIZE_ALIGN;
	a->committed_size = commit_size;
	a->commit_size = commit_size;
	a->reserve_size = reserve_size;

	return a;
}

void arena_release(arena *a) {
	plat_memory_release(a, a->reserve_size);
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

void *arena_alloc(arena *a, size_t size) {
	void *out = arena_alloc_align(a, size, ARENA_DEFAULT_ALIGNMENT);
	if (out)
		memset(out, 0, size);

	return out;
}

void arena_dealloc(arena *a, size_t size) {
	if (size == 0)
		return;

	size_t position = a->position;
	size_t new_pos = (uint64_t)position - size < ARENA_HEADER_SIZE_ALIGN
		? ARENA_HEADER_SIZE_ALIGN : (uint64_t)position - size;

	size_t aligned_commit_boundary = ALIGN_UP_POW2(new_pos, a->commit_size);
	if (aligned_commit_boundary < a->committed_size) {
		size_t decommit_size = a->committed_size - aligned_commit_boundary;
		uint8_t *decommit_ptr = (uint8_t*)a + aligned_commit_boundary;
		if (!plat_memory_decommit(decommit_ptr, decommit_size))
			return;

		a->committed_size = aligned_commit_boundary;
	}

	a->position = new_pos;
}

void arena_dealloc_to(arena *a, size_t position) {
	size_t size = position < a->position ? a->position - position : 0;
	arena_dealloc(a, size);
}

#define ARRAY_LENGTH(arr) (sizeof(arr)/sizeof(arr[0]))

static THREAD_LOCAL arena *s_scratch_arenas[2];
static inline arena *scratch_arena(int index) {
	assert(index >= 0 && index < (int)ARRAY_LENGTH(s_scratch_arenas));
	return s_scratch_arenas[index];
}

static inline void set_scratch_arena(int index, arena *arena) {
	assert(index >= 0 && index < (int)ARRAY_LENGTH(s_scratch_arenas));
	s_scratch_arenas[index] = arena;
}

void arena_scratch_alloc(void) {
	for (size_t i = 0; i < ARRAY_LENGTH(s_scratch_arenas); i++) {
		if (!scratch_arena(i))
			set_scratch_arena(i, arena_new(ARENA_DEFAULT_RESERVE_SIZE, ARENA_DEFAULT_COMMIT_SIZE));
	}
}

void arena_scratch_release(void) {
	for (size_t i = 0; i < ARRAY_LENGTH(s_scratch_arenas); i++) {
		arena_release(scratch_arena(i));
		set_scratch_arena(i, NULL);
	}
}

arena_temp arena_scratch_begin(arena **conflicts, int conflict_count) {
	arena_temp scratch = { 0 };
	for (size_t i = 0; i < ARRAY_LENGTH(s_scratch_arenas); i++) {
		int is_conflict = 0;
		for (int j = 0; j < conflict_count; j++) {
			arena *conflict = conflicts[j];
			if (scratch_arena(i) == conflict) {
				is_conflict = 1;
				break;
			}
		}
		if (!is_conflict) {
			scratch.arena = scratch_arena(i);
			scratch.position = scratch.arena->position;
			break;
		}
	}

	return scratch;
}

void arena_scratch_end(arena_temp scratch) {
	arena_dealloc_to(scratch.arena, scratch.position);
}
