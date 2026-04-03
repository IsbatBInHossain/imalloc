#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>

#define CHUNK_REQUEST_SIZE 64 * 1024
#define MAX_ALLOCATION CHUNK_REQUEST_SIZE
#define ALIGN16(x) (((x) + 15) & ~15)

// Flag and size macros
#define FLAG_FREE 0x1
#define FLAG_MMAP 0x2

#define GET_SIZE(b) ((b)->size_n_flags & ~0xF)
#define GET_FREE(b) ((b)->size_n_flags & FLAG_FREE)
#define GET_MAP(b) ((b)->size_n_flags & FLAG_MMAP)

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
  if (GET_MAP(block))
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
  printf("Sizeof header: %zu\n", sizeof(block_t));
  void *a = imalloc(45 * 1024);
  void *b = imalloc(32 * 1024);
  void *c = imalloc(17);
  print_heap();

  ifree(a);
  print_heap();
  ifree(c);
  print_heap();
  ifree(b);

  print_heap();
}