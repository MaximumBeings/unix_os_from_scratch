/* Chapter 55: a long run of the SIMULATOR against the correct node, under AddressSanitizer + UBSan: SEEDS seeds from FIRST, 300 ticks each (the four fault profiles, 3, 5 and 7 nodes), every safety property checked after every tick. No violation may appear.
 * Usage: raft_sweep FIRST SEEDS */
#include <stdio.h>
#include <stdlib.h>
#include "../055_sim.h"
static sim_t S;
int main(int argc, char **argv) {
    if (argc < 3) { return 2; } uint32_t first = (uint32_t)atol(argv[1]), cnt = (uint32_t)atol(argv[2]); uint32_t viol = 0, el = 0, pr = 0, ak = 0, cr = 0, rs = 0, dl = 0, dr = 0, dup = 0, mc = 0, cm1 = 0, nolead = 0; uint32_t prof[4] = {0};
    for (uint32_t k = 0; k < cnt; k++) { uint32_t seed = first + k; sim_cfg_t c; sim_config(&c, seed); sim_run(&S, &c, seed); if (S.viol) { viol++; printf("VIOLATION seed %u: %s at tick %u\n", seed, sim_viol_name(S.viol), S.viol_tick); }
        el += S.elections; pr += S.proposals; ak += S.acked; cr += S.crashes; rs += S.restarts; dl += S.delivered; dr += S.dropped; dup += S.dups; if (S.max_commit > mc) { mc = S.max_commit; } if (S.max_commit >= 2) { cm1++; } if (!S.elections) { nolead++; } prof[seed % 4]++; }
    printf("%u seeds (%u per profile): %u violations; %u elections, %u client commands proposed and %u acknowledged, %u crashes and %u restarts, %u messages delivered, %u dropped, %u duplicated; the largest commit index reached was %u; %u runs committed something beyond the first no-op, %u runs never elected a leader\n", cnt, prof[0], viol, el, pr, ak, cr, rs, dl, dr, dup, mc, cm1, nolead);
    return viol ? 1 : 0;
}
