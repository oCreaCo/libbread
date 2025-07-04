#ifndef BREAD_HASHTABLE_H
#define BREAD_HASHTABLE_H

#include <stdint.h>

#define NUM_HASH_ENTRY_SHIFT	(7)
#define NUM_HASH_ENTRY			(1 << NUM_HASH_ENTRY_SHIFT)
#define HASH_ENTRY_MASK			(NUM_HASH_ENTRY - 1)

extern int bread_init_hashtable();
extern int bread_hashtable_lookup(uint64_t func_fingerprint, int stack_depth);
extern int bread_hashtable_insert(uint64_t func_fingerprint, int node_idx);
extern int bread_hashtable_delete(uint64_t func_fingerprint); // might be removed

#endif /* BREAD_HASHTABLE_H */
