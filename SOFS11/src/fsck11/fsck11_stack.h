typedef struct element {
  uint32_t val;
  uint32_t parent;
  struct element *next;
} stackElement;

typedef struct stack {
  stackElement *head;
  uint32_t size;
} FSCKStack;

FSCKStack* newStack ();
void destroyStack (FSCKStack* stack);
int isEmpty (FSCKStack* stack);
void stackIn (FSCKStack* stack, uint32_t val, uint32_t parent);
uint32_t getNextParent(FSCKStack* stack);
uint32_t stackOut (FSCKStack* stack);
