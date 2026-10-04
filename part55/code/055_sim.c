/* Chapter 55: the Raft simulator (see 055_sim.h). Freestanding. The order in which random numbers are drawn is part of the specification: raft_ref.py makes the same draws in the same order. */
#include "055_sim.h"

static uint32_t rnd(sim_t *s) { s->rs = s->rs * 1664525u + 1013904223u; return s->rs >> 8; }
static void mix(sim_t *s, uint32_t v) { s->trace = (s->trace ^ (v & 0xFF)) * 16777619u; s->trace = (s->trace ^ ((v >> 8) & 0xFF)) * 16777619u; s->trace = (s->trace ^ ((v >> 16) & 0xFF)) * 16777619u; s->trace = (s->trace ^ ((v >> 24) & 0xFF)) * 16777619u; }
const char *sim_viol_name(int v) { switch (v) { case 0: return "none"; case 1: return "election safety (two leaders in one term)"; case 2: return "log matching"; case 3: return "state machine safety (a committed entry differs)"; case 4: return "leader completeness (a leader lacks a committed entry)";
    case 5: return "durability (an acknowledged command is not committed)"; case 6: return "monotonicity (a term or commit index went backwards)"; } return "?"; }
void sim_config(sim_cfg_t *c, uint32_t seed) {
    static const int NS[3] = {3, 5, 7}; c->n = NS[(seed / 4) % 3]; c->ticks = 300; c->propose_pm = 80; c->drop_pm = 0; c->dup_pm = 0; c->delay_max = 3; c->part_pm = 0; c->heal_pm = 60; c->crash_pm = 0; c->restart_pm = 0; c->target_pm = 0;
    switch (seed % 4) { case 0: break; /* a calm network */
        case 1: c->drop_pm = 200; c->dup_pm = 100; c->delay_max = 9; break; /* a lossy, reordering network */
        case 2: c->part_pm = 20; c->heal_pm = 50; c->delay_max = 5; break; /* partitions */
        default: c->drop_pm = 100; c->dup_pm = 60; c->delay_max = 7; c->part_pm = 15; c->heal_pm = 50; c->crash_pm = 30; c->restart_pm = 80; c->target_pm = 70; break; } /* everything at once */
}
static void save(sim_t *s, int i) { if (s->node[i].dirty) { s->disk[i] = s->node[i].d; s->node[i].dirty = 0; s->saves++; } }
static void push(sim_t *s, const raft_msg_t *m) { if (s->nq >= SIM_QMAX) { s->qfull++; return; } uint32_t delay = 1 + rnd(s) % s->cfg.delay_max; s->q[s->nq].at = s->t + delay; s->q[s->nq].m = *m; s->nq++; }
static void send(sim_t *s, const raft_msg_t *m) { s->sent++; if (rnd(s) % 1000 < s->cfg.drop_pm) { s->dropped++; return; } int copies = 1; if (rnd(s) % 1000 < s->cfg.dup_pm) { copies = 2; s->dups++; } for (int k = 0; k < copies; k++) { push(s, m); } }
static int linked(const sim_t *s, int a, int b) { return !s->partitioned || s->group[a] == s->group[b]; }
static void start_node(sim_t *s, int i, int fresh) { raft_init(&s->node[i], i, s->cfg.n, fresh ? 0 : &s->disk[i], rnd(s)); s->alive[i] = 1; s->last_commit[i] = 0; }
void sim_start(sim_t *s, const sim_cfg_t *c, uint32_t seed) {
    uint8_t *z = (uint8_t *)s; for (uint32_t i = 0; i < sizeof *s; i++) { z[i] = 0; } s->cfg = *c; s->rs = seed * 2654435761u + 12345u; s->trace = 2166136261u; s->next_cmd = 1;
    for (int i = 0; i < c->n; i++) { s->disk[i].voted_for = -1; start_node(s, i, 1); }
}
static void viol(sim_t *s, int v, int node) { if (!s->viol) { s->viol = v; s->viol_tick = s->t; s->viol_node = node; } }
static void check(sim_t *s) {
    int n = s->cfg.n; uint32_t maxterm = 0;
    for (int i = 0; i < n; i++) { uint32_t tm = s->alive[i] ? s->node[i].d.term : s->disk[i].term; if (tm > maxterm) { maxterm = tm; } }
    for (int i = 0; i < n; i++) { if (!s->alive[i]) { continue; } const raft_t *r = &s->node[i];
        if (r->d.term < s->last_term[i]) { viol(s, 6, i); } s->last_term[i] = r->d.term; if (r->commit < s->last_commit[i]) { viol(s, 6, i); } s->last_commit[i] = r->commit; if (r->commit > s->max_commit) { s->max_commit = r->commit; }
        if (r->role == RAFT_LEADER && r->d.term < SIM_TERMS) { if (s->leader_of_term[r->d.term] == 0) { s->leader_of_term[r->d.term] = (uint8_t)(i + 1); s->elections++; if (!s->first_leader_tick) { s->first_leader_tick = s->t; } } else if (s->leader_of_term[r->d.term] != i + 1) { viol(s, 1, i); } } }
    for (int i = 0; i < n; i++) { for (int j = i + 1; j < n; j++) { if (!s->alive[i] || !s->alive[j]) { continue; } const raft_t *a = &s->node[i], *b = &s->node[j]; uint32_t m = a->d.log_len < b->d.log_len ? a->d.log_len : b->d.log_len;
            for (uint32_t k = m; k >= 1; k--) { if (a->d.log[k - 1].term == b->d.log[k - 1].term) { for (uint32_t q = 1; q <= k; q++) { if (a->d.log[q - 1].term != b->d.log[q - 1].term || a->d.log[q - 1].cmd != b->d.log[q - 1].cmd) { viol(s, 2, i); } } break; } } } }
    for (int i = 0; i < n; i++) { if (!s->alive[i]) { continue; } const raft_t *r = &s->node[i];
        for (uint32_t k = 1; k <= r->commit && k <= s->gc_len; k++) { if (k > r->d.log_len || r->d.log[k - 1].term != s->gc[k - 1].term || r->d.log[k - 1].cmd != s->gc[k - 1].cmd) { viol(s, 3, i); } }
        while (s->gc_len < r->commit && s->gc_len < RAFT_LOG_MAX) { if (r->d.log_len <= s->gc_len) { viol(s, 3, i); break; } s->gc[s->gc_len].term = r->d.log[s->gc_len].term; s->gc[s->gc_len].cmd = r->d.log[s->gc_len].cmd; s->gc[s->gc_len].ct = maxterm; s->gc_len++; } }
    for (int i = 0; i < n; i++) { if (!s->alive[i] || s->node[i].role != RAFT_LEADER) { continue; } const raft_t *r = &s->node[i];
        for (uint32_t k = 1; k <= s->gc_len; k++) { if (r->d.term > s->gc[k - 1].ct && (r->d.log_len < k || r->d.log[k - 1].term != s->gc[k - 1].term || r->d.log[k - 1].cmd != s->gc[k - 1].cmd)) { viol(s, 4, i); } } }
    for (int k = 0; k < s->np; k++) { sim_prop_t *p = &s->p[k]; const raft_t *r = &s->node[p->node];
        if (!p->acked && s->alive[p->node] && r->role == RAFT_LEADER && r->d.term == p->term && r->commit >= p->idx && r->d.log[p->idx - 1].term == p->term && r->d.log[p->idx - 1].cmd == p->cmd) { p->acked = 1; s->acked++; }
        if (p->acked && (s->gc_len < p->idx || s->gc[p->idx - 1].cmd != p->cmd)) { viol(s, 5, p->node); } }
}
int sim_step(sim_t *s) {
    int n = s->cfg.n; s->t++; raft_msg_t out[RAFT_MAX_NODES];
    uint32_t r = rnd(s); if (!s->partitioned) { if (r % 1000 < s->cfg.part_pm) { s->partitioned = 1; for (int i = 0; i < n; i++) { s->group[i] = (int)(rnd(s) % 3); } } } else if (r % 1000 < s->cfg.heal_pm) { s->partitioned = 0; }
    r = rnd(s); if (r % 1000 < s->cfg.crash_pm) { int i = (int)(rnd(s) % (uint32_t)n); if (s->cfg.target_pm && rnd(s) % 100 < s->cfg.target_pm) { for (int k = 0; k < n; k++) { if (s->alive[k] && s->node[k].role == RAFT_LEADER) { i = k; break; } } } /* an adversary that likes to kill leaders */ if (s->alive[i]) { s->alive[i] = 0; s->crashes++; } }
    r = rnd(s); if (r % 1000 < s->cfg.restart_pm) { int i = (int)(rnd(s) % (uint32_t)n); if (!s->alive[i]) { start_node(s, i, 0); s->restarts++; } }
    r = rnd(s); if (r % 1000 < s->cfg.propose_pm) { int i = (int)(rnd(s) % (uint32_t)n); uint32_t idx; if (s->alive[i] && s->node[i].role == RAFT_LEADER && s->np < SIM_PMAX && raft_propose(&s->node[i], s->next_cmd, &idx) == RAFT_OK) {
            s->p[s->np].idx = idx; s->p[s->np].term = s->node[i].d.term; s->p[s->np].cmd = s->next_cmd; s->p[s->np].node = i; s->p[s->np].acked = 0; s->np++; s->next_cmd++; s->proposals++; save(s, i); } }
    int nq = s->nq; for (int k = 0; k < nq; k++) { if (s->q[k].at != s->t) { continue; } raft_msg_t m = s->q[k].m; uint32_t rr = rnd(s);
        if (s->alive[m.to] && linked(s, m.from, m.to)) { s->delivered++; mix(s, s->t); mix(s, m.from); mix(s, m.to); mix(s, m.type); mix(s, m.term); mix(s, m.nent + m.success + m.granted); int c = raft_recv(&s->node[m.to], &m, rr, out); save(s, m.to); for (int j = 0; j < c; j++) { send(s, &out[j]); } } }
    { int w = 0; for (int k = 0; k < s->nq; k++) { if (s->q[k].at > s->t) { s->q[w++] = s->q[k]; } } s->nq = w; }
    for (int i = 0; i < n; i++) { if (!s->alive[i]) { continue; } int c = raft_tick(&s->node[i], rnd(s), out); save(s, i); for (int j = 0; j < c; j++) { send(s, &out[j]); } }
    for (int i = 0; i < n; i++) { if (s->alive[i]) { mix(s, (uint32_t)i); mix(s, (uint32_t)s->node[i].role); mix(s, s->node[i].d.term); mix(s, s->node[i].d.log_len); mix(s, s->node[i].commit); } }
    check(s); return s->viol;
}
int sim_run(sim_t *s, const sim_cfg_t *c, uint32_t seed) { sim_start(s, c, seed); for (uint32_t k = 0; k < c->ticks && !s->viol; k++) { sim_step(s); } return s->viol; }
