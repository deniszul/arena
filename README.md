# Arena Allocator implementation in C

Arena Allocator in C design for compability across 3 primary different platform (Windows, Linux, and Mac) with reliable features and convinience macros also it use Virtual Memory rather than using malloc/free allowing for reserving large address space but only commit memory that program needs

## Quick Start
To use this library you can just simply copy [`arena.h`](./arena.h) and [`arena.c`](./arena.c) to your project and `#include` the header in your source file

## **Usage Examples**
### Basic Usage
```c
#include "arena.h"

int main(void) {
    arena *a = arena_new(MiB(1), KiB(4));

    int *array = ALLOC_ARRAY(a, int, 4);
    array[0] = 1;
    array[1] = 2;
    array[2] = 3;
    array[3] = 4;

    arena_release(a);
    return 0;
}
```

### Temporary Scope
```c
void example(arena *a) {
    arena_temp temp = arena_temp_begin(a);

    char *buffer = ALLOC_ARRAY(temp.arena, char, 128);
    snprintf(buffer, 128, "temporary string");

    // use buffer...

    arena_temp_end(temp); // all allocations since begin are rolled back
}
```

### Non-Zeroed allocation
```c
typedef struct {
    float x, y;
} Vec2;

Vec2 *v = ALLOC_STRUCT_NZ(a, Vec2);
v->x = 1.0f;
v->y = 2.0f;
```

### Scratch Arena
```c
arena_scratch_alloc();

arena *conflicts[] = { main_arena };
arena_temp scratch = arena_scratch_begin(conflicts, 1);

char *tmp = ALLOC_ARRAY(scratch.arena, char, 256);

// use tmp...

arena_scratch_end(scratch);
arena_scratch_release();
```

This function and macro use ARENA_DEFAULT_ALIGNMENT for alignment
```c
arena_alloc(arena*, size_t size);
arena_alloc_nz(arena, size);
```

## Configuration
You can adjust the default values by modifying these macros in [`arena.h`](./arena.h)
```c
#define ARENA_DEFAULT_ALIGNMENT 8           // Default alignment
#define ARENA_DEFAULT_RESERVE_SIZE MiB(1)   // Default initial reserved size
#define ARENA_DEFAULT_COMMIT_SIZE KiB(64)   // Default initial commit size
```

# Inspiration
- Ryan Fleury [`article`](https://www.dgtlgrove.com/p/untangling-lifetimes-the-arena-allocator)
- The Stack Frame [`youtube video`](https://youtu.be/ONMdF1AuCBg?si=hAgQCIpoaJrPzd4e)
- MagicalBat [`youtube video`](https://youtu.be/jgiMagdjA1s?si=VgR0Gt5tB7mgAr20)
