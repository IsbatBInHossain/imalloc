#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct block
{
  size_t size;
  bool free;
  struct block *next;
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
  block_t *block = find_free_block(size); // returns block_t*, not void*

  if (block == NULL)
  {
    void *request = sbrk(sizeof(block_t) + size);
    if (request == (void *)-1)
      return NULL;

    block = (block_t *)request;
    block->size = size;
    block->free = false;
    block->next = NULL;

    // Only add to list if it's a NEW block
    if (head == NULL)
    {
      head = block;
    }
    else
    {
      block_t *curr = head;
      while (curr->next != NULL)
        curr = curr->next;
      curr->next = block;
    }
  }
  else
  {
    // Reusing existing block — just mark it used
    block->free = false;
  }

  return (void *)(block + 1);
}

void ifree(void *ptr)
{
  if (!ptr)
    return;

  block_t *block = (block_t *)ptr - 1;
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
  void *a = imalloc(20);
  void *b = imalloc(20);
  void *c = imalloc(20);
  print_heap();

  ifree(a);
  ifree(c);
  ifree(b);

  print_heap();
}