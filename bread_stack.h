#ifndef BREAD_STACK_H
#define BREAD_STACK_H

typedef struct StackEntry
{
    int node_idx;
} StackEntry;

extern int bread_init_stack();
extern int bread_stack_push(int node_idx);
extern int bread_stack_pop();

#endif /* BREAD_STACK_H */