#include <libbread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include "bread.h"
#include "bread_hashtable.h"

#ifndef OUTSTREAM
#define OUTSTREAM stderr
#endif

thread_local int flag = 0;
thread_local uint64_t gene_prev;
thread_local uint64_t gene;

thread_local int tid = -1;
thread_local int t_type;
thread_local uint64_t t_token;

thread_local int num_nodes = 0;
thread_local BreadStatNodeData bread_nodes[MAX_NUM_NODES];

thread_local int stack[MAX_NUM_NODES]; // node_idx
thread_local int most_recent_node_idx;
thread_local int stack_depth;
thread_local int print_depth;

thread_local char print_buffer[1 << 16];
thread_local int print_len;

#ifdef BREAD_MEM
thread_local int64_t mem_curr;
thread_local int64_t mem_peak;

void *(*real_malloc)(size_t) = NULL;
void (*real_free)(void*) = NULL;
#endif /* BREAD_MEM */

#ifdef BREAD_IO
thread_local ssize_t data_read;
thread_local ssize_t data_write;

ssize_t (*real_pread)(int, void*, size_t, off_t) = NULL;
ssize_t (*real_pwrite)(int, const void*, size_t, off_t) = NULL;
#endif /* BREAD_IO */

#ifdef BREAD_MEM
void *malloc(size_t size)
{
	void *ptr;

    if (unlikely(real_malloc == NULL)) {
        real_malloc = dlsym(RTLD_NEXT, "malloc");

        if (unlikely(real_malloc == NULL)) {
            fprintf(stderr, "Failed to find real malloc\n");
            return NULL;
        }
    }

    ptr = real_malloc(size);

    if (flag) {
        BreadStatNode curr = bread_nodes + stack[stack_depth];
        size_t mem_alloced = malloc_usable_size(ptr);

        curr->mem_alloced += mem_alloced;
        mem_curr += mem_alloced;

        if (mem_peak < mem_curr)
            mem_peak = mem_curr;
    }

    return ptr;
}

void free(void *ptr)
{
    if (unlikely(real_free == NULL)) {
        real_free = dlsym(RTLD_NEXT, "free");

        if (unlikely(real_free == NULL)) {
            fprintf(stderr, "Failed to find real free\n");
            return;
        }
    }

    if (flag) {
        BreadStatNode curr = bread_nodes + stack[stack_depth];
        size_t mem_freed = malloc_usable_size(ptr);

        curr->mem_freed += mem_freed;
        mem_curr -= mem_freed;
    }

    real_free(ptr);
}
#endif /* BREAD_MEM */

#ifdef BREAD_IO
ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
	ssize_t size;

    if (unlikely(real_pread == NULL)) {
        real_pread = dlsym(RTLD_NEXT, "pread");

        if (unlikely(real_pread == NULL)) {
            fprintf(stderr, "Failed to find real pread\n");
            return 0;
        }
    }

    if (flag) {
        struct timespec start, end;
        BreadStatNode curr = bread_nodes + stack[stack_depth];

        clock_gettime(CLOCK_MONOTONIC, &(start));
        size = real_pread(fd, buf, count, offset);
        clock_gettime(CLOCK_MONOTONIC, &(end));

        curr->data_read += size;
        curr->io_read_latency +=
            ((end.tv_sec * G + end.tv_nsec) -
             (start.tv_sec * G + start.tv_nsec));
        data_read += size;
    } else {
        size = real_pread(fd, buf, count, offset);
    }

    return size;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	ssize_t size;

    if (unlikely(real_pwrite == NULL)) {
        real_pwrite = dlsym(RTLD_NEXT, "pwrite");

        if (unlikely(real_pwrite == NULL)) {
            fprintf(stderr, "Failed to find real pwrite\n");
            return 0;
        }
    }

    if (flag) {
        struct timespec start, end;
        BreadStatNode curr = bread_nodes + stack[stack_depth];

        clock_gettime(CLOCK_MONOTONIC, &(start));
        size = real_pwrite(fd, buf, count, offset);
        clock_gettime(CLOCK_MONOTONIC, &(end));

        curr->data_write += size;
        curr->io_write_latency +=
            ((end.tv_sec * G + end.tv_nsec) -
             (start.tv_sec * G + start.tv_nsec));
        data_write += size;
    } else {
        size = real_pwrite(fd, buf, count, offset);
    }

    return size;
}
#endif /* BREAD_IO */

int bread_init_internal(int is_main, uint64_t token)
{
    if (unlikely(flag == 0))
        return 1;

    if (unlikely(bread_init_hashtable()))
        return 1;

    if (unlikely(tid == -1))
        tid = gettid();

    gene_prev = -1;
    gene = 0;
    
    t_type = is_main;
    t_token = token;

    num_nodes = 0;
    most_recent_node_idx = -1;
    stack_depth = -1;

#ifdef BREAD_MEM
    mem_curr = 0;
    mem_peak = 0;
#endif /* BREAD_MEM */

#ifdef BREAD_IO
    data_read = 0;
    data_write = 0;
#endif /* BREAD_IO */

    for (int i = 0; i < MAX_NUM_NODES; i++) {
        bread_nodes[i].hash_next = -1;
        bread_nodes[i].stack_depth = -1;
        bread_nodes[i].num_children = 0;
        bread_nodes[i].recursion_cnt = 0;

        bread_nodes[i].call_cnt = 0;

        bread_nodes[i].latency = 0;
#ifdef BREAD_IO
        bread_nodes[i].io_read_latency = 0;
        bread_nodes[i].io_write_latency = 0;
#endif /* BREAD_IO */

#ifdef BREAD_MEM
        bread_nodes[i].mem_alloced = 0;
        bread_nodes[i].mem_freed = 0;
#endif /* BREAD_MEM */

#ifdef BREAD_IO
        bread_nodes[i].data_read = 0;
        bread_nodes[i].data_write = 0;
#endif /* BREAD_IO */

        stack[i] = -1;
    }

    return 0;
}

int bread_start_internal(const char *func)
{
    uint64_t func_fingerprint;
    BreadStatNode curr;
    int node_idx;

    if (unlikely(flag == 0))
        return 1;

    if (likely(stack_depth != -1)) {
        if (unlikely(stack_depth == MAX_NUM_NODES - 1)) {
            fprintf(stderr, "bread_start failed: max call depth\n");
            return 1;
        }

        func_fingerprint = (uint64_t)(func) << stack_depth;

        /* Recursion */
        if ((gene_prev ^ func_fingerprint) == gene)
        {
            curr = bread_nodes + most_recent_node_idx;
            ++(curr->recursion_cnt);
            ++(curr->call_cnt);
            return 0;
        }

        func_fingerprint <<= 1;
        ++stack_depth;
    } else {
        func_fingerprint = (uint64_t)(func) << (++stack_depth);
    }

    gene_prev = gene;
    gene ^= func_fingerprint;
    node_idx = bread_hashtable_lookup(gene);

    if (unlikely(node_idx == -1)) {
        if (unlikely(num_nodes == MAX_NUM_NODES)) {
            fprintf(stderr, "bread_start failed: max bread nodes\n");
            for (int i = 0; i < MAX_NUM_NODES; i++)
                fprintf(stderr, "depth: %d, %s\n", bread_nodes[i].stack_depth, bread_nodes[i].func_name);
            return 1;
        }

        node_idx = num_nodes++;

        curr = bread_nodes + node_idx;
        curr->stack_depth = stack_depth;
        curr->gene_prev = gene_prev;
        curr->gene = gene;
        curr->func_fingerprint = func_fingerprint;
        sprintf(curr->func_name, func);

        if (likely(stack_depth != 0)) {
            BreadStatNode parent = bread_nodes + stack[stack_depth - 1];

            if (unlikely(parent->num_children == MAX_NUM_CHILD)) {
                fprintf(stderr, "bread_start failed: max child nodes\n");
                for (int i = 0; i < MAX_NUM_CHILD; i++)
                    fprintf(stderr, "func: %s\n", bread_nodes[parent->children[i]].func_name);
                return 1;
            }

            parent->children[parent->num_children++] = node_idx;
        }

        bread_hashtable_insert(gene, node_idx);
    } else {
        curr = bread_nodes + node_idx;
    }

    ++(curr->recursion_cnt);
    ++(curr->call_cnt);
    stack[stack_depth] = node_idx;
    most_recent_node_idx = node_idx;

    clock_gettime(CLOCK_MONOTONIC, &(curr->start_ts));

    return 0;
}

int bread_end()
{
    BreadStatNode curr;

    if (unlikely(flag == 0))
        return 1;

    curr = bread_nodes + most_recent_node_idx;
    
    /* During recursion */
    if (--(curr->recursion_cnt) > 0)
        return 0;

    clock_gettime(CLOCK_MONOTONIC, &(curr->end_ts));

    curr->latency +=
        ((curr->end_ts.tv_sec * G + curr->end_ts.tv_nsec) -
         (curr->start_ts.tv_sec * G + curr->start_ts.tv_nsec));

    /* parent */
    most_recent_node_idx = stack[--stack_depth];
    curr = bread_nodes + most_recent_node_idx;

    gene_prev = curr->gene_prev;
    gene = curr->gene;

    return 0;
}

static void bread_print_node(BreadStatNode curr)
{
    uint64_t tmp1, tmp2;

    for (int i = 0; i < print_depth; i++)
        print_len += sprintf(print_buffer + print_len, "    ");

    print_len += sprintf(print_buffer + print_len, "[%s]\n",
            curr->func_name);

    /* latency */
    {
        for (int i = 0; i < print_depth; i++)
            print_len += sprintf(print_buffer + print_len, "    ");
        print_len += sprintf(print_buffer + print_len, "<latency: ");
        tmp1 = curr->latency;

        tmp2 = tmp1 / G;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu s ", tmp2);
        tmp1 %= G;

        tmp2 = tmp1 / M;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu ms ", tmp2);
        tmp1 %= M;

        tmp2 = tmp1 / K;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu us ", tmp2);
        tmp1 %= K;

        print_len += sprintf(print_buffer + print_len, "%lu ns, depth: %d>\n",
                             tmp1, curr->stack_depth);
    }

#ifdef BREAD_IO
    /* io_read_latency */
    {
        for (int i = 0; i < print_depth; i++)
            print_len += sprintf(print_buffer + print_len, "    ");
        print_len += sprintf(print_buffer + print_len, " ├─ io_read_latency: ");
        tmp1 = curr->io_read_latency;

        tmp2 = tmp1 / G;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu s ", tmp2);
        tmp1 %= G;

        tmp2 = tmp1 / M;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu ms ", tmp2);
        tmp1 %= M;

        tmp2 = tmp1 / K;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu us ", tmp2);
        tmp1 %= K;

        print_len += sprintf(print_buffer + print_len, "%lu ns\n", tmp1);
    }

    /* io_write_latency */
    {
        for (int i = 0; i < print_depth; i++)
            print_len += sprintf(print_buffer + print_len, "    ");
        print_len += sprintf(print_buffer + print_len, " ├─ io_write_latency: ");
        tmp1 = curr->io_write_latency;

        tmp2 = tmp1 / G;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu s ", tmp2);
        tmp1 %= G;

        tmp2 = tmp1 / M;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu ms ", tmp2);
        tmp1 %= M;

        tmp2 = tmp1 / K;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu us ", tmp2);
        tmp1 %= K;

        print_len += sprintf(print_buffer + print_len, "%lu ns\n", tmp1);
    }
#endif /* BREAD_IO */

#ifdef BREAD_MEM
    /* mem_alloced */
    {
        for (int i = 0; i < print_depth; i++)
            print_len += sprintf(print_buffer + print_len, "    ");
        print_len += sprintf(print_buffer + print_len, " ├─ mem_alloced: ");
        tmp1 = curr->mem_alloced;

        tmp2 = tmp1 / GB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu G ", tmp2);
        tmp1 %= GB;

        tmp2 = tmp1 / MB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu M ", tmp2);
        tmp1 %= MB;

        tmp2 = tmp1 / KB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu K ", tmp2);
        tmp1 %= KB;

        print_len += sprintf(print_buffer + print_len, "%lu B\n", tmp1);
    }

    /* mem_freed */
    {
        for (int i = 0; i < print_depth; i++)
            print_len += sprintf(print_buffer + print_len, "    ");
        print_len += sprintf(print_buffer + print_len, " ├─ mem_freed: ");
        tmp1 = curr->mem_freed;

        tmp2 = tmp1 / GB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu G ", tmp2);
        tmp1 %= GB;

        tmp2 = tmp1 / MB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu M ", tmp2);
        tmp1 %= MB;

        tmp2 = tmp1 / KB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu K ", tmp2);
        tmp1 %= KB;

        print_len += sprintf(print_buffer + print_len, "%lu B\n", tmp1);
    }
#endif /* BREAD_MEM */

#ifdef BREAD_IO
    /* data_read */
    {
        for (int i = 0; i < print_depth; i++)
            print_len += sprintf(print_buffer + print_len, "    ");
        print_len += sprintf(print_buffer + print_len, " ├─ data_read: ");
        tmp1 = curr->data_read;

        tmp2 = tmp1 / GB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu G ", tmp2);
        tmp1 %= GB;

        tmp2 = tmp1 / MB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu M ", tmp2);
        tmp1 %= MB;

        tmp2 = tmp1 / KB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu K ", tmp2);
        tmp1 %= KB;

        print_len += sprintf(print_buffer + print_len, "%lu B\n", tmp1);
    }

    /* data_write */
    {
        for (int i = 0; i < print_depth; i++)
            print_len += sprintf(print_buffer + print_len, "    ");
        print_len += sprintf(print_buffer + print_len, " ├─ data_write: ");
        tmp1 = curr->data_write;

        tmp2 = tmp1 / GB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu G ", tmp2);
        tmp1 %= GB;

        tmp2 = tmp1 / MB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu M ", tmp2);
        tmp1 %= MB;

        tmp2 = tmp1 / KB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu K ", tmp2);
        tmp1 %= KB;

        print_len += sprintf(print_buffer + print_len, "%lu B\n", tmp1);
    }
#endif /* BREAD_IO */

    /* call_cnt */
    {
        for (int i = 0; i < print_depth; i++)
            print_len += sprintf(print_buffer + print_len, "    ");
        print_len += sprintf(print_buffer + print_len, " └─ call_cnt: %lu\n", curr->call_cnt);
    }

    for (int i = 0; i < curr->num_children; i++) {
        ++print_depth;
        bread_print_node(bread_nodes + curr->children[i]);
        --print_depth;
    }
}

static void bread_add_child_stat(BreadStatNode curr)
{
    BreadStatNode child;

    for (int i = 0; i < curr->num_children; i++) {
        child = bread_nodes + curr->children[i];
        bread_add_child_stat(child);

#ifdef BREAD_IO
        curr->io_read_latency += child->io_read_latency;
        curr->io_write_latency += child->io_write_latency;
#endif /* BREAD_IO */

#ifdef BREAD_MEM
        curr->mem_alloced += child->mem_alloced;
        curr->mem_freed += child->mem_freed;
#endif /* BREAD_MEM */

#ifdef BREAD_IO
        curr->data_read += child->data_read;
        curr->data_write += child->data_write;
#endif /* BREAD_IO */
    }
}

int bread_finish()
{
    uint64_t tmp1, tmp2;
    char file_name[128];
    FILE *output_file;

    if (unlikely(flag == 0))
        return 1;
    
    if (unlikely(bread_nodes[0].stack_depth == -1)) // no measurement
        return 0;

    print_len = 0;
    print_len += sprintf(print_buffer + print_len, "bread_finish, tid: %d, type: %s",
                         tid, t_type == 1 ? "main" : "worker");

#ifdef BREAD_MEM
    /* mem_peak */
    {
        print_len += sprintf(print_buffer + print_len, ", mem_peak: ");
        tmp1 = mem_peak;

        tmp2 = tmp1 / GB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu G ", tmp2);
        tmp1 %= GB;

        tmp2 = tmp1 / MB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu M ", tmp2);
        tmp1 %= MB;

        tmp2 = tmp1 / KB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu K ", tmp2);
        tmp1 %= KB;

        print_len += sprintf(print_buffer + print_len, "%lu B", tmp1);
    }
#endif /* BREAD_MEM */

#ifdef BREAD_IO
    /* data_read */
    {
        print_len += sprintf(print_buffer + print_len, ", data_read: ");
        tmp1 = data_read;

        tmp2 = tmp1 / GB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu G ", tmp2);
        tmp1 %= GB;

        tmp2 = tmp1 / MB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu M ", tmp2);
        tmp1 %= MB;

        tmp2 = tmp1 / KB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu K ", tmp2);
        tmp1 %= KB;

        print_len += sprintf(print_buffer + print_len, "%lu B", tmp1);
    }

    /* data_write */
    {
        print_len += sprintf(print_buffer + print_len, ", data_write: ");
        tmp1 = data_write;

        tmp2 = tmp1 / GB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu G ", tmp2);
        tmp1 %= GB;

        tmp2 = tmp1 / MB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu M ", tmp2);
        tmp1 %= MB;

        tmp2 = tmp1 / KB;
        if (tmp2 > 0)
            print_len += sprintf(print_buffer + print_len, "%lu K ", tmp2);
        tmp1 %= KB;

        print_len += sprintf(print_buffer + print_len, "%lu B", tmp1);
    }
#endif /* BREAD_IO */
    if (t_token != 0)
        print_len += sprintf(print_buffer + print_len, ", token: %lu", t_token);
    print_len += sprintf(print_buffer + print_len, "\n");

    // DFS
    bread_add_child_stat(bread_nodes);

    // DFS
    print_depth = 0;
    bread_print_node(bread_nodes);

#if 0 /* TODO */
    sprintf(file_name, "%s/libbread_results/%d.%ld.txt", getenv("HOME"), tid, t_token);
    output_file = fopen(file_name, "a+");
    fprintf(output_file, print_buffer);
    fclose(output_file);
#endif
    fprintf(OUTSTREAM, print_buffer);

    return 0;
}

int bread_is_flag_on()
{
    return flag;
}

void bread_flag_on()
{
    flag = 1;
}

void bread_flag_off()
{
    flag = 0;
}
