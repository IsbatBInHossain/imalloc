#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>

#define CHUNK_REQUEST_SIZE 64 * 1024
#define MAX_ALLOCATION CHUNK_REQUEST_SIZE
#define ALIGN16(x) (((x) + 15) & ~15)

typedef struct block
{
  size_t size;
  bool free;
  struct block *next;
  bool is_mmap;
} block_t;

block_t *head = NULL;

block_t *allocate_free_memory(block_t *block, size_t size)
{
  // Only split if the leftover is large enough to be useful
  if (block->size >= size + sizeof(block_t) + 1)
  {
    block_t *next = (block_t *)((char *)(block + 1) + size);
    next->size = block->size - size - sizeof(block_t);
    next->free = true;
    next->next = block->next;
    block->next = next;
    block->size = size;
  }
  // else: reuse the whole block as-is (internal fragmentation, but safe)
  return block;
}

block_t *find_free_block(size_t size)
{
  if (head == NULL)
    return NULL;
  block_t *curr = head;
  while (curr != NULL)
  {
    if (curr->size >= size && curr->free == true)
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
    block->size = size;
    block->is_mmap = true;
    block->free = false;
    return (void *)(block + 1);
  }

  block_t *block = find_free_block(ALIGN16(size));

  while (block == NULL)
  {
    // Request must fit at least the header + size, use CHUNK_REQUEST_SIZE or size if larger
    size_t chunk_size = ALIGN16(sizeof(block_t) + size);
    if (chunk_size < CHUNK_REQUEST_SIZE)
      chunk_size = CHUNK_REQUEST_SIZE;

    void *request = sbrk(chunk_size);
    if (request == (void *)-1)
      return NULL; // out of memory

    block_t *chunk = (block_t *)request;
    chunk->size = chunk_size - sizeof(block_t);
    chunk->free = true;
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

    block = find_free_block(ALIGN16(size));
    if (!block)
    {
      printf("Failed\n");
    }
  }

  block->free = false;
  return (void *)(block + 1);
}

void ifree(void *ptr)
{
  if (!ptr)
    return;

  block_t *block = (block_t *)ptr - 1;
  if (block->is_mmap)
  {
    size_t total_size = ALIGN16(sizeof(block_t) + block->size);
    munmap(block, total_size);
    return;
  }
  block->free = true;
  // Coalesce consecutive free blocks backward
  block_t *curr = head;
  block_t *prev = NULL;

  while (curr && curr != block)
  {
    prev = curr;
    curr = curr->next;
  }

  if (prev && prev->free == true)
  {
    prev->size += sizeof(block_t) + block->size;
    prev->next = block->next;
    block = prev;
  }

  // Coalesce consecutive free blocks forward
  block_t *next = block->next;
  while (next && next->free == true)
  {
    block->size += sizeof(block_t) + next->size;
    block->next = next->next;
    next = next->next;
  }

  printf("Freed block:\n");
  printf("address : %p\n", (void *)block);
  printf("size    : %zu\n", block->size);
}

void print_heap()
{
  block_t *curr = head;
  while (curr)
  {
    printf("[addr=%p size=%zu free=%d] -> ",
           (void *)curr, curr->size, curr->free);
    curr = curr->next;
  }
  printf("NULL\n");
}

int main()
{
  void *a = imalloc(80 * 1024);
  void *b = imalloc(32);
  void *c = imalloc(17);
  print_heap();

  ifree(a);
  ifree(c);
  ifree(b);

  print_heap();
}