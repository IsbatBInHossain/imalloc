#include <stdio.h>
#include <unistd.h>

typedef struct block
{
  size_t size;
  int free;
  struct block *next;
} block_t;

block_t *head = NULL;

void *imalloc(size_t size)
{
  void *request = sbrk(sizeof(block_t) + size);
  if (request == (void *)-1)
    return NULL;

  block_t *block = (block_t *)request;

  block->size = size;
  block->free = 0;
  block->next = NULL;

  // Add to linked list
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
  int *x = imalloc(sizeof(int));
  int *y = imalloc(sizeof(int));

  print_heap();

  ifree(x);

  print_heap();
}