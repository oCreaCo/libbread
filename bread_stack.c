#include "bread_stack.h"
#include "bread.h"

thread_local int stack_depth;
thread_local StackEntry stack[MAX_NUM_NODES];

int bread_init_stack()
{
    stack_depth = 0;

	for (int i = 0; i < MAX_NUM_NODES; i++)
		stack[i].node_idx = -1;

    return 0;
}

int bread_stack_push(int node_idx)
{
    stack[stack_depth++].node_idx = node_idx;

    return 0;
}