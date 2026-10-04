/* Chapter 55: the Raft node (see 055_raft.h). Freestanding: no libc, no libgcc. */
#include "055_raft.h"

int raft_bug = RAFT_BUG_NONE;

static int popcount(uint32_t v) { int c = 0; while (v) { c += (int)(v & 1u); v >>= 1; } return c; }
static uint32_t last_index(const raft_t *r) { return r->d.log_len; }
static uint32_t term_at(const raft_t *r, uint32_t idx) { return idx == 0 ? 0 : r->d.log[idx - 1].term; }
static void reset_timer(raft_t *r, uint32_t rnd) { r->elapsed = 0; r->timeout = RAFT_ELECT_MIN + rnd % RAFT_ELECT_RANGE; }
static void apply_committed(raft_t *r) { while (r->applied < r->commit && r->applied < r->d.log_len) { r->applied++; uint32_t c = r->d.log[r->applied - 1].cmd; if (c != 0) { r->sm_count++; r->sm_hash = (r->sm_hash ^ c) * 16777619u; } } }
static void become_follower(raft_t *r, uint32_t term) { if (term > r->d.term) { r->d.term = term; r->d.voted_for = -1; r->dirty = 1; } r->role = RAFT_FOLLOWER; r->votes = 0; }
void raft_init(raft_t *r, int id, int n, const raft_disk_t *disk, uint32_t rnd) {
    uint8_t *p = (uint8_t *)r; for (uint32_t i = 0; i < sizeof *r; i++) { p[i] = 0; } r->id = id; r->n = n; r->leader = -1; r->role = RAFT_FOLLOWER; r->d.voted_for = -1;
    if (disk) { r->d = *disk; if (raft_bug == RAFT_BUG_VOTE_NOT_PERSISTED) { r->d.voted_for = -1; } } /* the bug: the vote was only ever kept in memory, so a restart forgets it */ reset_timer(r, rnd); r->sm_hash = 2166136261u;
}
static void make_ae(const raft_t *r, int to, raft_msg_t *m) {
    uint32_t nx = r->next[to] < 1 ? 1 : r->next[to]; uint32_t prev = nx - 1; /* nextIndex is never below 1 */ uint32_t cnt = last_index(r) >= nx ? last_index(r) - nx + 1 : 0; if (cnt > RAFT_MAX_ENT) { cnt = RAFT_MAX_ENT; }
    uint8_t *z = (uint8_t *)m; for (uint32_t i = 0; i < sizeof *m; i++) { z[i] = 0; } m->type = RAFT_MSG_AE; m->from = (uint8_t)r->id; m->to = (uint8_t)to; m->term = r->d.term; m->prev_idx = prev; m->prev_term = term_at(r, prev); m->commit = r->commit; m->nent = cnt;
    for (uint32_t i = 0; i < cnt; i++) { m->ent[i] = r->d.log[nx - 1 + i]; }
}
static int broadcast_ae(raft_t *r, raft_msg_t *out) { int k = 0; for (int i = 0; i < r->n; i++) { if (i != r->id) { make_ae(r, i, &out[k++]); } } return k; }
static void advance_commit(raft_t *r) {
    for (uint32_t idx = last_index(r); idx > r->commit; idx--) {
        int same_term = r->d.log[idx - 1].term == r->d.term || raft_bug == RAFT_BUG_COMMIT_OLD_TERM; /* the paper's Figure 8 rule: only an entry of the leader's own term is committed by counting replicas */
        if (!same_term) { continue; } int c = 1; for (int i = 0; i < r->n; i++) { if (i != r->id && r->match[i] >= idx) { c++; } }
        if (raft_bug == RAFT_BUG_MINORITY_COMMIT ? c >= r->n / 2 : c > r->n / 2) { r->commit = idx; break; }
    }
    apply_committed(r);
}
int raft_tick(raft_t *r, uint32_t rnd, raft_msg_t *out) {
    r->elapsed++;
    if (r->role == RAFT_LEADER) { r->hb++; if (r->hb >= RAFT_HEARTBEAT) { r->hb = 0; return broadcast_ae(r, out); } return 0; }
    if (r->elapsed < r->timeout) { return 0; }
    r->d.term++; r->d.voted_for = r->id; r->dirty = 1; r->role = RAFT_CANDIDATE; r->votes = 1u << r->id; r->leader = -1; reset_timer(r, rnd); int k = 0;
    for (int i = 0; i < r->n; i++) { if (i == r->id) { continue; } raft_msg_t *m = &out[k++]; uint8_t *z = (uint8_t *)m; for (uint32_t j = 0; j < sizeof *m; j++) { z[j] = 0; } m->type = RAFT_MSG_RV; m->from = (uint8_t)r->id; m->to = (uint8_t)i; m->term = r->d.term; m->last_idx = last_index(r); m->last_term = term_at(r, last_index(r)); }
    return k;
}
static void reply(const raft_t *r, const raft_msg_t *req, raft_msg_t *m) { uint8_t *z = (uint8_t *)m; for (uint32_t i = 0; i < sizeof *m; i++) { z[i] = 0; } m->from = (uint8_t)r->id; m->to = req->from; m->term = r->d.term; }
int raft_recv(raft_t *r, const raft_msg_t *m, uint32_t rnd, raft_msg_t *out) {
    if (m->to != r->id || m->from >= r->n || m->from == r->id) { return 0; }
    if (m->term > r->d.term) { become_follower(r, m->term); }
    if (m->type == RAFT_MSG_RV) {
        raft_msg_t *o = &out[0]; reply(r, m, o); o->type = RAFT_MSG_RVR; o->granted = 0;
        if (m->term == r->d.term && (r->d.voted_for == -1 || r->d.voted_for == m->from)) {
            uint32_t lt = term_at(r, last_index(r)), li = last_index(r); int up_to_date = m->last_term > lt || (m->last_term == lt && m->last_idx >= li); /* the candidate's log is at least as up to date as ours */
            if (raft_bug == RAFT_BUG_NO_LOG_CHECK_IN_VOTE) { up_to_date = 1; }
            if (up_to_date) { r->d.voted_for = m->from; r->dirty = 1; o->granted = 1; reset_timer(r, rnd); }
        }
        o->term = r->d.term; return 1;
    }
    if (m->type == RAFT_MSG_RVR) {
        if (r->role != RAFT_CANDIDATE || m->term != r->d.term || !m->granted) { return 0; }
        r->votes |= 1u << m->from; if (popcount(r->votes) <= r->n / 2) { return 0; }
        r->role = RAFT_LEADER; r->leader = r->id; r->hb = 0; for (int i = 0; i < r->n; i++) { r->next[i] = last_index(r) + 1; r->match[i] = 0; }
        if (last_index(r) < RAFT_LOG_MAX) { r->d.log[r->d.log_len].term = r->d.term; r->d.log[r->d.log_len].cmd = 0; r->d.log_len++; r->dirty = 1; r->match[r->id] = last_index(r); } /* the no-op of a new leader */
        return broadcast_ae(r, out);
    }
    if (m->type == RAFT_MSG_AE) {
        raft_msg_t *o = &out[0]; reply(r, m, o); o->type = RAFT_MSG_AER; o->success = 0;
        if (m->term < r->d.term) { return 1; }
        r->role = RAFT_FOLLOWER; r->leader = m->from; reset_timer(r, rnd);
        if (raft_bug != RAFT_BUG_NO_CONSISTENCY_CHECK && (m->prev_idx > last_index(r) || (m->prev_idx > 0 && term_at(r, m->prev_idx) != m->prev_term))) { return 1; } /* the consistency check fails: the leader will back up */
        for (uint32_t k = 0; k < m->nent; k++) {
            uint32_t idx = m->prev_idx + 1 + k;
            if (idx <= last_index(r) && r->d.log[idx - 1].term != m->ent[k].term) { r->d.log_len = idx - 1; r->dirty = 1; } /* a conflicting entry: delete it and all that follow */
            if (idx > last_index(r)) { if (r->d.log_len >= RAFT_LOG_MAX) { return 1; } r->d.log[r->d.log_len++] = m->ent[k]; r->dirty = 1; }
        }
        uint32_t mi = m->prev_idx + m->nent; uint32_t nc = m->commit < mi ? m->commit : mi; if (nc > r->commit || raft_bug == RAFT_BUG_COMMIT_REGRESS) { r->commit = nc; apply_committed(r); } /* only ever forward: a delayed, reordered message must not move commitIndex back */
        o->success = 1; o->match = mi; return 1;
    }
    if (m->type == RAFT_MSG_AER) {
        if (r->role != RAFT_LEADER || m->term != r->d.term) { return 0; }
        if (m->success) { if (m->match > r->match[m->from]) { r->match[m->from] = m->match; } r->next[m->from] = r->match[m->from] + 1; advance_commit(r); return 0; }
        if (r->next[m->from] > 1) { r->next[m->from]--; } make_ae(r, m->from, &out[0]); return 1; /* back up one entry and try again at once */
    }
    return 0;
}
int raft_propose(raft_t *r, uint32_t cmd, uint32_t *index) {
    if (r->role != RAFT_LEADER) { return RAFT_ERR_NOT_LEADER; } if (cmd == 0 || r->d.log_len >= RAFT_LOG_MAX) { return RAFT_ERR_FULL; }
    r->d.log[r->d.log_len].term = r->d.term; r->d.log[r->d.log_len].cmd = cmd; r->d.log_len++; r->dirty = 1; r->match[r->id] = last_index(r); if (index) { *index = last_index(r); } return RAFT_OK;
}
