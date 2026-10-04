/* Chapter 55: a DETERMINISTIC SIMULATOR for Raft. Every source of nondeterminism a real cluster has (message loss, duplication, delay and reordering, network partitions, crashes and restarts, when clients speak, every node's timeout)
 * is drawn from one seeded pseudo-random generator, so a seed fully determines a run: a failure found on seed 4,217 is replayed exactly by running seed 4,217 again. The simulator plays the network, the disks and the clients, and after EVERY tick
 * it checks the safety properties of the Raft paper (Figure 3) against a global view no real node has:
 *   violation 1  ELECTION SAFETY       two different leaders in one term
 *   violation 2  LOG MATCHING          two logs hold an entry with the same index and term but differ in it or before it
 *   violation 3  STATE MACHINE SAFETY  two nodes (or one node at two times) hold different entries at one COMMITTED index
 *   violation 4  LEADER COMPLETENESS   a leader of a later term lacks an entry committed in an earlier term
 *   violation 5  DURABILITY            a client command that was acknowledged (committed at its leader) is not in the committed log
 *   violation 6  MONOTONICITY          a term, or a commit index of a node that has not restarted, went backwards
 * The disk is modelled as an atomic save of the persistent state after every node step, before any message of that step is sent (the rule the paper requires). The network is a lossy queue; links are cut by partitions. */
#ifndef SIM_H
#define SIM_H
#include <stdint.h>
#include "055_raft.h"

#define SIM_QMAX 256
#define SIM_PMAX 128
#define SIM_TERMS 256
typedef struct { uint32_t drop_pm, dup_pm, delay_max, part_pm, heal_pm, crash_pm, restart_pm, propose_pm, ticks, target_pm; int n; } sim_cfg_t; /* probabilities in parts per thousand per tick or per message */
typedef struct { uint32_t term, cmd, ct; } sim_gc_t;
typedef struct { uint32_t idx, term, cmd; int node, acked; } sim_prop_t;
typedef struct { uint32_t at; raft_msg_t m; } sim_q_t;
typedef struct {
    sim_cfg_t cfg; uint32_t t, rs, trace, next_cmd; raft_t node[RAFT_MAX_NODES]; raft_disk_t disk[RAFT_MAX_NODES]; int alive[RAFT_MAX_NODES], group[RAFT_MAX_NODES], partitioned;
    sim_q_t q[SIM_QMAX]; int nq; sim_gc_t gc[RAFT_LOG_MAX]; uint32_t gc_len; uint8_t leader_of_term[SIM_TERMS]; sim_prop_t p[SIM_PMAX]; int np;
    uint32_t last_commit[RAFT_MAX_NODES], last_term[RAFT_MAX_NODES];
    int viol; uint32_t viol_tick; int viol_node; /* the first violation found, 0 if none */
    uint32_t sent, delivered, dropped, dups, elections, crashes, restarts, proposals, acked, saves, max_commit, first_leader_tick, qfull;
} sim_t;
void sim_config(sim_cfg_t *c, uint32_t seed); /* the fault profile of a seed: its node count (3, 5 or 7), its mix of faults, its length */
void sim_start(sim_t *s, const sim_cfg_t *c, uint32_t seed);
int sim_step(sim_t *s);                       /* one tick; returns 0 while no violation has been found */
int sim_run(sim_t *s, const sim_cfg_t *c, uint32_t seed); /* runs c->ticks ticks or until a violation; returns the violation code (0 = none) */
const char *sim_viol_name(int v);
#endif
