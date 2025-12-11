# libbread: library for performance breakdown
A lightweight, low-overhead performance-analysis library for instrumenting function-level execution behavior in C/C++ programs.

libbread provides fine-grained metrics such as execution time, optional I/O latency, memory allocation volume, and flamegraph-compatible traces.  
Its goal is to offer a simple but extensible interface for analyzing performance without modifying compiler toolchains or runtime environments.

---

## 1. What libbread Measures

libbread records **performance metrics for each instrumented function**, capturing execution characteristics of the code paths chosen to be monitored.

---

## 2. Performance Metrics

libbread supports multiple classes of metrics. Which metrics are enabled is controlled at compile time via `CPPFLAGS`.

### Execution Time (default)
By default, libbread measures **wall-clock execution time** for every instrumented function or code section.

### I/O Metrics (optional)

Enable by adding the `-DBREAD_IO` to `CPPFLAGS` in the Makefile:

- Latency of `pread()` and `pwrite()` operations  
- Amount of data read or written  

### Memory Metrics (optional)

Enable by adding the `-DBREAD_MEM` to `CPPFLAGS` in the Makefile:

- Total allocated memory  
- Total freed memory  

This allows insight into memory churn and allocation behavior across functions.

---

## 3. Output Format

libbread supports two categories of output: **human-readable logs** and **flamegraph-compatible traces**.

### Printed Output

Enable printed logs by adding the `-DPRINT` to `CPPFLAGS` in the Makefile. You may also specify the output stream: `-DOUTSTREAM=stdout` (default: stderr)

### Flamegraph Output

To generate output files compatible with the FlameGraph toolset, add `-DFLAME_GRAPH` to the Makefile. When enabled, libbread produces one output file per metric class formatted for direct use with `flamegraph.pl`.

---

## 4. How to Use

See **`test.c`** for a complete usage example.

### Enable / Disable Measurement

```c
int main(void)
{
    bread_flag_on();
    ...

    // measure performance metrics

    ...
    bread_flag_off();
}
```

Metrics are collected **only when the flag is ON**.

### Measure Function Execution

#### Option 1: Manual region measurement

```c
int func1(void)
{
    ...

    bread_start();

    // measurement target region

    bread_end();

    ...
    return 0;
}
```

or

```c
int func2(void)
{
    bread_start();
    ...

    if (error_condition) {
        bread_end();
        return 1;
    }

    ...
    bread_end();
    return 0;
}
```

`bread_start()` and `bread_end()` must appear as a pair, and `bread_end()` must always execute before the function returns.

#### Option 2: Measure the entire function

```c
int func(void)
{
    bread_trace();
    ...

    if (error_condition) {
        return 1;
    }

    ...
    return 0;
}
```

Place `bread_trace()` at the top of the function to automatically measure the entire function body.

---

### Initialize and Finalize Measurement

```c
int callee(void)
{
    bread_trace();

    ...

    return 0;
}

int caller(void)
{
    bread_init(); // Reset all stored metrics
    ...

    result = callee();

    ...
    bread_finish(); // Print or emit output files
    return result;
}
```

`bread_finish()` must be called after all measurements are complete.

---

### Set Output Directory (for FlameGraph mode)

If `-DFLAME_GRAPH` is enabled:

```c
bread_set_output_directory("/path/to/output");
```

All flamegraph-formatted files will be generated in this directory.