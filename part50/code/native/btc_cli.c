/* Chapter 50 host tool, built from the SAME 050_btc.c the kernel links.
 *   btc_cli block FILE [limit_bits_hex]   the canonical report for one raw block (what btc_ref.py prints)
 *   btc_cli tx FILE                       parse one raw transaction: 'check ok' or the reason, or 'unparseable <why>' */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../050_btc.h"
static btc_block_t B; static btc_tx_t T; static char out[1 << 20]; static uint8_t buf[1 << 22];
int main(int argc, char **argv) {
    if (argc < 3) { return 2; }
    FILE *f = fopen(argv[2], "rb"); if (!f) { perror("open"); return 2; } size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
    if (!strcmp(argv[1], "tx")) { uint32_t nx; int rc = btc_tx_parse(buf, (uint32_t)n, 0, &T, &nx); if (rc) { printf("unparseable %s\n", btc_strerror(rc)); return 0; } printf("check %s\n", nx != n ? "trailing" : T.check ? T.check : "ok"); return 0; }
    uint32_t lim = argc > 3 ? (uint32_t)strtoul(argv[3], 0, 16) : 0x1d00ffffu; int rc = btc_block_parse(&B, buf, (uint32_t)n, lim);
    if (rc) { printf("block %s size %zu unparseable %s\n", strrchr(argv[2], '/') ? strrchr(argv[2], '/') + 1 : argv[2], n, btc_strerror(rc)); return 0; }
    if (btc_report(out, sizeof out, strrchr(argv[2], '/') ? strrchr(argv[2], '/') + 1 : argv[2], &B) < 0) { return 3; } fputs(out, stdout); return 0;
}
