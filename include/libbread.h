#ifndef LIBBREAD_H
#define LIBBREAD_H

#include <stddef.h>
#include <stdint.h>
#define bread_init() bread_init_internal(1, 0)
#define bread_init_worker() bread_init_internal(0, 0)
#define bread_init_token(token) bread_init_internal(1, token)
#define bread_init_worker_token(token) bread_init_internal(0, token)
#define bread_start() bread_start_internal(__PRETTY_FUNCTION__)
#define bread_end() bread_end_internal(NULL)
#define bread_end_w_comment(comment) bread_end_internal(comment)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int bread_init_internal(int is_main, uint64_t token);
extern int bread_start_internal(const char *func);
extern int bread_end_internal(char *comment);
extern int bread_finish();

extern int bread_is_flag_on();
extern void bread_flag_on();
extern void bread_flag_off();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LIBBREAD_H */
