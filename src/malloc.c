#include <stdio.h>
#include <unistd.h>

void *imalloc(size_t size)
{
  void *ptr = sbrk(size);
  if (ptr == (void *)-1)
    return NULL;
  return ptr;
};
// void ifree(void *ptr);

int main()
{
  int *x = imalloc(sizeof(int));
  *x = 42;
  printf("%d\n", *x);
}