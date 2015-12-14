#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "fsck11_stack.h"

FSCKStack *newStack()
{
  FSCKStack* stack = (FSCKStack*) malloc(sizeof(FSCKStack));
  stack->head = NULL;
  stack->size = 0;
  return stack;
}

void destroyStack (FSCKStack* stack)
{
  while (!isEmpty(stack))
    stackOut(stack);
  free(stack);
}

int isEmpty(FSCKStack* stack)
{
  return (stack->size == 0) ? 1 : 0;
}

void stackIn(FSCKStack* stack, uint32_t val)
{
  stackElement* elem = (stackElement*) malloc(sizeof(stackElement));
  elem->val = val;
  elem->next = stack->head;
  stack->head = elem;
}

uint32_t stackOut(FSCKStack* stack)
{
  if (isEmpty(stack))
    return 0;

  stackElement* elem = stack->head;
  int ret = elem->val;
  stack->head = elem->next;
  free(elem);
  return ret;
}

