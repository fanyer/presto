#ifndef DEFLATE_CONFIG_H
#define DEFLATE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*****************************************************************************/
/*                  block_state - originally in deflate.c                    */
/*****************************************************************************/

typedef enum {
    need_more,      /* block not completed, need more input or more output */
    block_done,     /* block flush performed */
    finish_started, /* finish started, need only more output at next deflate */
    finish_done     /* finish done, accept no more input or output */
} block_state;

/*****************************************************************************/
/*                 compress_func - originally in deflate.c                   */
/*****************************************************************************/

typedef block_state (*compress_func) OF((deflate_state *s, int flush, int clas));
/* Compression function. Returns the block state after the call. */

/*****************************************************************************/
/*                   config_s - originally in deflate.c                      */
/*****************************************************************************/

typedef struct config_s {
   ush good_length; /* reduce lazy search above this match length */
   ush max_lazy;    /* do not perform lazy search above this match length */
   ush nice_length; /* quit search above this match length */
   ush max_chain;
   compress_func func;
} config;

void InitZLibDeflateConfigurationTable(config* conf);
config* get_deflate_config();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // DEFLATE_CONFIG_H
