#ifdef BREAD
#include <libbread.h>
#endif /* BREAD */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

typedef struct tmp
{
    int a;
    int b;
} tmp;

void A();
void B();
void C();
void D();
void E(int cnt);
void F(int cnt);
void G(int cnt);

void func(tmp * t)
{
#ifdef BREAD
    bread_start();
#endif /* BREAD */

    if (t->a % 2 == 0)
        fprintf(stderr, "even, b: %d\n", t->b);
    else
        fprintf(stderr, "odd, b: %d\n", t->b);

#ifdef BREAD
    bread_end();
#endif /* BREAD */
}

void G(int cnt)
{
#ifdef BREAD
    bread_start();
#endif /* BREAD */

    fprintf(stderr, "G, cnt: %d\n", cnt--);
    if (cnt > 0)
        G(cnt);

#ifdef BREAD
    bread_end();
#endif /* BREAD */
}

void F(int cnt)
{
#ifdef BREAD
    bread_start();
#endif /* BREAD */

    fprintf(stderr, "F, cnt: %d\n", cnt--);
    if (cnt > 0)
        E(cnt);

#ifdef BREAD
    bread_end();
#endif /* BREAD */
}

void E(int cnt)
{
#ifdef BREAD
    bread_start();
#endif /* BREAD */

    fprintf(stderr, "E, cnt: %d\n", cnt);
    F(cnt);

#ifdef BREAD
    bread_end();
#endif /* BREAD */
}

void D()
{
    fprintf(stderr, "D\n");
}

void C()
{
#ifdef BREAD
    bread_start();
#endif /* BREAD */

    fprintf(stderr, "C\n");

#ifdef BREAD
    bread_end();
#endif /* BREAD */
}

void B()
{
#ifdef BREAD
    bread_start();
#endif /* BREAD */

    fprintf(stderr, "B\n");
    C();

#ifdef BREAD
    bread_end();
#endif /* BREAD */
}

void A()
{
#ifdef BREAD
    bread_start();
#endif /* BREAD */

    fprintf(stderr, "A\n");
    B();

#ifdef BREAD
    bread_end();
#endif /* BREAD */
}

int main(int argc, char *argv[])
{
    tmp *arr[16];
    int fd;
    char buffer[32];
    int n;

    struct timespec start_1;
    struct timespec start_2;
    struct timespec end_1;
    struct timespec end_2;

    if (argc == 1)
        n = 8; // default
    else if (argc == 2)
        n = atoi(argv[1]);
    else
        return 1;

    if (n > 16)
        return 1;

#ifdef BREAD
    bread_flag_on();
    bread_init();
    bread_start();
#endif /* BREAD */

    for (int i = 0; i < n; i++) {
        tmp *t = (tmp*)malloc(sizeof(tmp));
        t->a = i;
        t->b = i*i;
        arr[i] = t;
    }

    for (int i = 0; i < n; i++)
        func(arr[i]);

    for (int i = 0; i < n; i++)
        free(arr[i]);

    A();
    B();
    E(5);
    G(10);

    fd = open("./testfile.txt", O_RDWR);
    pread(fd, buffer, 32, 0);
    pwrite(fd, "WRITE", 5, 0);

#ifdef BREAD
    bread_end();
    bread_finish();
#endif /* BREAD */

    clock_gettime(CLOCK_MONOTONIC, &start_1);
    clock_gettime(CLOCK_MONOTONIC, &start_2);
    fprintf(stderr, "ttttt\n");
    clock_gettime(CLOCK_MONOTONIC, &end_2);
    clock_gettime(CLOCK_MONOTONIC, &end_1);

    fprintf(stderr, "timer 1: %lu\n",
            ((end_1.tv_sec * 1000000000 + end_1.tv_nsec) -
             (start_1.tv_sec * 1000000000 + start_1.tv_nsec)));
    fprintf(stderr, "timer 2: %lu\n",
            ((end_2.tv_sec * 1000000000 + end_2.tv_nsec) -
             (start_2.tv_sec * 1000000000 + start_2.tv_nsec)));

    clock_gettime(CLOCK_MONOTONIC, &start_1);
    clock_gettime(CLOCK_MONOTONIC, &start_2);
    D();
    clock_gettime(CLOCK_MONOTONIC, &end_2);
    clock_gettime(CLOCK_MONOTONIC, &end_1);

    fprintf(stderr, "timer 1: %lu\n",
            ((end_1.tv_sec * 1000000000 + end_1.tv_nsec) -
             (start_1.tv_sec * 1000000000 + start_1.tv_nsec)));
    fprintf(stderr, "timer 2: %lu\n",
            ((end_2.tv_sec * 1000000000 + end_2.tv_nsec) -
             (start_2.tv_sec * 1000000000 + start_2.tv_nsec)));

#ifdef BREAD
    bread_init();
    bread_start();
#endif /* BREAD */

    clock_gettime(CLOCK_MONOTONIC, &start_1);
    clock_gettime(CLOCK_MONOTONIC, &start_2);
    C();
    clock_gettime(CLOCK_MONOTONIC, &end_2);
    clock_gettime(CLOCK_MONOTONIC, &end_1);

    fprintf(stderr, "timer 1: %lu\n",
            ((end_1.tv_sec * 1000000000 + end_1.tv_nsec) -
             (start_1.tv_sec * 1000000000 + start_1.tv_nsec)));
    fprintf(stderr, "timer 2: %lu\n",
            ((end_2.tv_sec * 1000000000 + end_2.tv_nsec) -
             (start_2.tv_sec * 1000000000 + start_2.tv_nsec)));

#ifdef BREAD
    bread_end_w_comment("comment test");
    bread_finish();
#endif /* BREAD */

    return 0;
}
