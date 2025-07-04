#ifndef BREAD_H
#define BREAD_H

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <threads.h>
#include <time.h>

#define MAX_NUM_NODES	(64)
#define MAX_NUM_CHILD	(8)

#define G (1000000000)
#define M (1000000)
#define K (1000)

#define GB (1 << 30)
#define MB (1 << 20)
#define KB (1 << 10)

#if !defined(likely) || !defined(unlikely)
#if __GNUC__ >= 3
#define likely(x)   __builtin_expect((x) != 0, 1)
#define unlikely(x) __builtin_expect((x) != 0, 0)
#else /* __GNUC__ < 3 */
#define likely(x)	((x) != 0)
#define unlikely(x) ((x) != 0)
#endif /* __GNUC__ >= 3 */
#endif /* !defined(likely) || !defined(unlikely) */

typedef struct BreadStatNodeData
{
	/* metadata */
	int hash_next; // node_idx
	int stack_depth;
	int num_children;
	int children[MAX_NUM_CHILD]; // node_idx
	uint64_t func_fingerprint;
	char func_name[256];

	/* etc. */
	uint64_t call_cnt;

	/* latency stat */
	uint64_t latency;
#ifdef BREAD_IO
	uint64_t io_read_latency;
	uint64_t io_write_latency;
#endif /* BREAD_IO */

#ifdef BREAD_MEM
	/* memory stat */
	size_t mem_alloced;
	size_t mem_freed;
#endif /* BREAD_MEM */

#ifdef BREAD_IO
	/* I/O stat */
	ssize_t data_read;
	ssize_t data_write;
#endif /* BREAD_IO */

	/* Timestamp */
	struct timespec start_ts;
	struct timespec end_ts;
} BreadStatNodeData;

typedef BreadStatNodeData *BreadStatNode;

extern thread_local BreadStatNodeData bread_nodes[];

#endif /* BREAD_H */
