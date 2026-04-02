#include <stdio.h>
#include <unistd.h>

typedef struct block
{
  size_t size;
  int free;
  struct block *next;
} block_t;

block_t *head = NULL;

block_t *find_free_block(size_t size)
{
  if (head == NULL)
    return NULL;
  block_t *curr = head;
  while (curr != NULL)
  {
    if (curr->size >= size && curr->free == 1)
    {
      return curr;
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
    block = sbrk(sizeof(block_t) + size);
    if (block == (void *)-1)
      return NULL;

    block->size = size;
    block->free = 0;
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
    block->free = 0;
  }

  return (void *)(block + 1);
}

void ifree(void *ptr)
{
  if (!ptr)
    return;

  block_t *block = (block_t *)ptr - 1;
  block->free = 1;

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
  long int *x = imalloc(sizeof(long int));
  int *y = imalloc(sizeof(int));

  print_heap();

  ifree(x);
  ifree(y);
  print_heap();
  int *z = imalloc(sizeof(int));

  print_heap();
}