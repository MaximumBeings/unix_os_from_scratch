/* Chapter 50: the embedded blocks and transaction vectors. */
#ifndef BTCDATA_H
#define BTCDATA_H
#include <stdint.h>
typedef struct { const char *name, *net; int height; const char *hash; uint32_t limit; const char *start, *end; } btc_blk_t;
#define N_BTC_BLOCKS 24
extern const btc_blk_t g_btc_blocks[N_BTC_BLOCKS];
extern const char *const g_btc_txvec_start, *const g_btc_txvec_end;
#endif
