#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>

#define CHUNK_REQUEST_SIZE 64 * 1024
#define MAX_ALLOCATION CHUNK_REQUEST_SIZE
#define MIN_SIZE 16
#define ALIGN16(x) (((x) + 15) & ~15)
#define MIN(a, b) ((a < b) ? (a) : (b))

// Flag and size macros
#define FLAG_FREE 0x1
#define FLAG_MMAP 0x2

#define GET_SIZE(b) ((b)->size_n_flags & ~0xF)
#define GET_FREE(b) ((b)->size_n_flags & FLAG_FREE)
#define GET_MMAP(b) ((b)->size_n_flags & FLAG_MMAP)

#define SET_FREE(b) ((b)->size_n_flags |= FLAG_FREE)
#define CLEAR_FREE(b) ((b)->size_n_flags &= ~FLAG_FREE)
#define SET_MMAP(b) ((b)->size_n_flags |= FLAG_MMAP)
#define CLEAR_MMAP(b) ((b)->size_n_flags &= ~FLAG_MMAP)
#define SET_SIZE(b, size) \
  ((b)->size_n_flags = (size) | ((b)->size_n_flags & 0xF))

typedef struct block
{
  size_t size_n_flags;
  struct block *next;
} block_t;

block_t *head = NULL;

block_t *allocate_free_memory(block_t *block, size_t size)
{
  // Only split if the leftover is large enough to hold metadata
  if (GET_SIZE(block) >= size + sizeof(block_t) + 1)
  {
    block_t *next = (block_t *)((char *)(block + 1) + size);
    next->size_n_flags = 0;
    SET_SIZE(next, GET_SIZE(block) - size - sizeof(block_t));
    SET_FREE(next);
    next->next = block->next;
    block->next = next;
    SET_SIZE(block, size);
  }
  // else: reuse the whole block
  return block;
}

block_t *find_free_block(size_t size)
{
  if (head == NULL)
    return NULL;
  block_t *curr = head;
  while (curr != NULL)
  {
    if (GET_SIZE(curr) >= size && GET_FREE(curr))
    {
      return allocate_free_memory(curr, size);
    }
    else
      curr = curr->next;
  }
  return NULL;
};

void *imalloc(size_t size)
{
  size = ALIGN16(size);
  if (size >= MAX_ALLOCATION)
  {
    size_t total_size = ALIGN16(sizeof(block_t) + size);
    void *request = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (request == (void *)-1)
    {
      return NULL;
    }
    block_t *block = request;
    block->size_n_flags = 0;
    SET_SIZE(block, size);
    SET_MMAP(block);
    CLEAR_FREE(block);
    return (void *)(block + 1);
  }

  block_t *block = find_free_block(ALIGN16(size));

  while (block == NULL)
  {
    // Request must fit at least the header + size
    size_t chunk_size = ALIGN16(sizeof(block_t) + size);
    if (chunk_size < CHUNK_REQUEST_SIZE)
      chunk_size = CHUNK_REQUEST_SIZE;

    void *request = sbrk(chunk_size);
    if (request == (void *)-1)
      return NULL; // out of memory

    block_t *chunk = (block_t *)request;
    chunk->size_n_flags = 0;

    SET_SIZE(chunk, chunk_size - sizeof(block_t));
    SET_FREE(chunk);
    CLEAR_MMAP(chunk);
    chunk->next = NULL;

    if (head == NULL)
    {
      head = chunk;
    }
    else
    {
      block_t *curr = head;
      while (curr->next != NULL)
        curr = curr->next;
      curr->next = chunk;
    }

    block = allocate_free_memory(chunk, ALIGN16(size));
  }

  CLEAR_FREE(block);
  return (void *)(block + 1);
}

void ifree(void *ptr)
{
  if (!ptr)
    return;

  block_t *block = (block_t *)ptr - 1;
  if (GET_MMAP(block))
  {
    size_t total_size = ALIGN16(sizeof(block_t) + GET_SIZE(block));
    munmap(block, total_size);
    return;
  }
  SET_FREE(block);
  // Coalesce consecutive free blocks backward
  block_t *curr = head;
  block_t *prev = NULL;

  while (curr && curr != block)
  {
    prev = curr;
    curr = curr->next;
  }

  if (prev && GET_FREE(prev) && ((char *)prev + sizeof(block_t) + GET_SIZE(prev) == (char *)block))
  {
    size_t total_size = GET_SIZE(prev) + sizeof(block_t) + GET_SIZE(block);
    SET_SIZE(prev, total_size);
    prev->next = block->next;
    block = prev;
  }

  // Coalesce consecutive free blocks forward
  block_t *next = block->next;
  while (next && GET_FREE(next) &&
         ((char *)block + sizeof(block_t) + GET_SIZE(block) == (char *)next))
  {
    size_t total_size = GET_SIZE(block) + sizeof(block_t) + GET_SIZE(next);
    SET_SIZE(block, total_size);
    block->next = next->next;
    next = next->next;
  }
}

void *irealloc(void *ptr, size_t size)
{
  // check the edge cases first
  if (ptr == NULL)
    return imalloc(size);
  if (size == 0)
  {
    ifree(ptr);
    return NULL;
  }
  block_t *old_block = (block_t *)ptr - 1;
  size_t old_size = GET_SIZE(old_block);
  size_t new_size = ALIGN16(size);
  // printf("Old size: %zu, New size: %zu\n", old_size, new_size);

  void *new_ptr = imalloc(new_size);
  if (!new_ptr)
    return NULL;
  memcpy(new_ptr, ptr, MIN(old_size, new_size));
  ifree(ptr);
  return new_ptr;
}

void print_heap()
{
  block_t *curr = head;
  while (curr)
  {
    printf("[addr=%p size=%zu free=%zu] -> ",
           (void *)curr, GET_SIZE(curr), GET_FREE(curr));
    curr = curr->next;
  }
  printf("NULL\n");
}

int main()
{
  printf("==== BASIC ALLOC/FREE ====\n");
  void *a = imalloc(32);
  void *b = imalloc(64);
  print_heap();

  ifree(a);
  ifree(b);
  print_heap();

  printf("\n==== REUSE TEST ====\n");
  void *c = imalloc(16);
  print_heap(); // should reuse previous freed blocks

  printf("\n==== SPLIT TEST ====\n");
  void *d = imalloc(128);
  ifree(d);
  print_heap();
  void *e = imalloc(32); // should split
  print_heap();

  printf("\n==== COALESCE TEST ====\n");
  void *x = imalloc(32);
  void *y = imalloc(32);
  void *z = imalloc(32);

  ifree(x);
  ifree(z);
  print_heap();

  ifree(y); // should merge all
  print_heap();

  printf("\n==== iREALLOC GROW ====\n");
  char *r = imalloc(16);
  strcpy(r, "hello");
  r = irealloc(r, 64);
  printf("iRealloc content: %s\n", r);
  print_heap();

  printf("\n==== iREALLOC SHRINK ====\n");
  r = irealloc(r, 8);
  printf("iRealloc content: %s\n", r);
  print_heap();

  printf("\n==== MMAP TEST ====\n");
  void *big = imalloc(70 * 1024); // should use mmap
  print_heap();
  ifree(big);

  printf("\n==== STRESS TEST ====\n");
  void *arr[100];
  for (int i = 0; i < 100; i++)
  {
    arr[i] = imalloc((i % 50) + 1);
  }

  for (int i = 0; i < 100; i += 2)
  {
    ifree(arr[i]);
  }

  for (int i = 1; i < 100; i += 2)
  {
    ifree(arr[i]);
  }

  print_heap();

  printf("\n==== EDGE CASES ====\n");
  void *null_test = irealloc(NULL, 32);
  print_heap();

  null_test = irealloc(null_test, 0); // should free
  print_heap();

  ifree(NULL); // should do nothing

  printf("\n==== DONE ====\n");
}