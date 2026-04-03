# imalloc

A simple custom memory allocator written in C.

The goal of this project is to understand how memory allocation works under the hood without skipping important details.

## Features

- 16-byte aligned allocations
- Free list with first-fit strategy
- Block splitting
- Forward and backward coalescing
- `sbrk`-based heap growth (64KB chunks)
- `mmap` for large allocations
- Bit-packed metadata (size + flags in one field)
- Basic `realloc` implementation

## Design Overview

### Allocation Strategy

- **Small allocations:**
  - Use `sbrk` to request memory in 64KB chunks
  - Maintain a linked list of blocks
  - Reuse free blocks using first-fit
- **Large allocations:**
  - Use `mmap`
  - Freed using `munmap`

### Block Layout

Each block has a header:

```c
typedef struct block
{
  size_t size_n_flags; // upper bits: size, lower bits: flags
  struct block *next;
} block_t;
```

- Size is 16-byte aligned
- Lower 4 bits are used for flags:
  - free / used
  - mmap / heap

User pointer:

```c
(void *)(block + 1)
```

### Free List

- Singly linked list
- Stores all heap blocks (both free and used)
- First-fit search

### Splitting

If a free block is larger than requested, split into:
- allocated block
- remaining free block

### Coalescing

On `free`:
- Merge with previous block if adjacent and free
- Merge with next block if adjacent and free

### Realloc

- Allocates new memory
- Copies old data
- Frees old block

## Build

```bash
make
```

Or manually:

```bash
gcc -Wall -Wextra -g src/malloc.c -o imalloc
```

## Run

```bash
./imalloc
```

## Testing

Run with Valgrind:

```bash
valgrind --leak-check=full --track-origins=yes ./imalloc
```

Expected result:
- 0 errors
- No memory leaks

## Limitations

- Not thread-safe
- Linear free list (O(n) search)
- `realloc` does not expand in-place
- No boundary tags (coalescing is not O(1))
- No protection against invalid frees

## Why This Project

This is a learning-focused allocator that covers:

- Memory layout
- Fragmentation
- Allocation strategies
- Low-level pointer arithmetic
- Interaction with the OS (`sbrk`, `mmap`)

## What Is Not Covered

- In-place `realloc` 
- Segregated free lists
- Boundary tags
- Thread-local arenas
- Debug features (canaries, guards)