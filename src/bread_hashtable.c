#include "bread_hashtable.h"
#include "bread.h"

thread_local int hashtable[NUM_HASH_ENTRY];

int bread_init_hashtable()
{
	for (int i = 0; i < NUM_HASH_ENTRY; i++)
		hashtable[i] = -1;

    return 0;
}

int bread_hashtable_lookup(uint64_t func_fingerprint, int stack_depth)
{
	int curr_idx = hashtable[(int)(func_fingerprint & HASH_ENTRY_MASK)];
	
	while (curr_idx != -1) {
		BreadStatNode curr = bread_nodes + curr_idx;
		if (curr->func_fingerprint == func_fingerprint &&
			curr->stack_depth == stack_depth)
			return curr_idx;

		curr_idx = curr->hash_next;
	}

	return -1;
}

int bread_hashtable_insert(uint64_t func_fingerprint, int node_idx)
{
	int *entry = hashtable + (int)(func_fingerprint & HASH_ENTRY_MASK); 
	bread_nodes[node_idx].hash_next = *entry;
	*entry = node_idx;

	return 0;
}
