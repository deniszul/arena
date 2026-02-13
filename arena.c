#include "arena.h"

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
