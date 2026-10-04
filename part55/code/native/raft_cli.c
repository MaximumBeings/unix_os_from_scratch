/* Chapter 55 host tool, built from the SAME 055_raft.c and 055_sim.c the kernel links. Usage: raft_cli FIRST_SEED LAST_SEED [BUG [TICKS]]. One line per seed: its profile, the first violation (if any), the statistics and the trace hash. BUG selects a deliberate bug (see 055_raft.h). */
#include <stdio.h>
#include <stdlib.h>
#include "../055_sim.h"
static sim_t S;
int main(int argc, char **argv) {
    if (argc < 3) { return 2; } uint32_t a = (uint32_t)atol(argv[1]), b = (uint32_t)atol(argv[2]); raft_bug = argc > 3 ? atoi(argv[3]) : 0; uint32_t ticks = argc > 4 ? (uint32_t)atol(argv[4]) : 0;
    for (uint32_t seed = a; seed <= b; seed++) { sim_cfg_t c; sim_config(&c, seed); if (ticks) { c.ticks = ticks; } sim_run(&S, &c, seed);
        printf("seed %u n %d viol %d tick %u node %d sent %u delivered %u dropped %u dups %u elections %u crashes %u restarts %u proposals %u acked %u saves %u maxcommit %u firstleader %u trace %08x\n", seed, c.n, S.viol, S.viol ? S.viol_tick : 0, S.viol ? S.viol_node : -1, S.sent, S.delivered, S.dropped, S.dups, S.elections, S.crashes, S.restarts, S.proposals, S.acked, S.saves, S.max_commit, S.first_leader_tick, S.trace); }
    return 0;
}
