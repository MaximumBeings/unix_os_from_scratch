/* Chapter 55 host test: the Raft node, driven by hand, including the two scenarios of the Raft paper that every implementation must get right (Figure 7: followers whose logs differ from the leader's in every way, and Figure 8: a leader must not
 * commit an entry of an earlier term by counting replicas). Under AddressSanitizer + UBSan. PASS/FAIL per line; exit status 1 on any FAIL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../055_raft.h"
static int pass_n, fail_n;
static void check(const char *name, int ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", name); if (ok) { pass_n++; } else { fail_n++; } }
static raft_msg_t out[RAFT_MAX_NODES];
static void log_of(raft_disk_t *d, uint32_t term, int32_t vote, int len, const uint32_t *terms) { memset(d, 0, sizeof *d); d->term = term; d->voted_for = vote; d->log_len = (uint32_t)len; for (int i = 0; i < len; i++) { d->log[i].term = terms[i]; d->log[i].cmd = 100 + (uint32_t)i; } }
static raft_msg_t rv(int from, int to, uint32_t term, uint32_t li, uint32_t lt) { raft_msg_t m; memset(&m, 0, sizeof m); m.type = RAFT_MSG_RV; m.from = (uint8_t)from; m.to = (uint8_t)to; m.term = term; m.last_idx = li; m.last_term = lt; return m; }
static raft_msg_t ae(int from, int to, uint32_t term, uint32_t pi, uint32_t pt, uint32_t commit, int nent, const raft_ent_t *e) { raft_msg_t m; memset(&m, 0, sizeof m); m.type = RAFT_MSG_AE; m.from = (uint8_t)from; m.to = (uint8_t)to; m.term = term; m.prev_idx = pi; m.prev_term = pt; m.commit = commit; m.nent = (uint32_t)nent; for (int i = 0; i < nent; i++) { m.ent[i] = e[i]; } return m; }
static int same_log(const raft_t *a, const raft_t *b) { return a->d.log_len == b->d.log_len && !memcmp(a->d.log, b->d.log, a->d.log_len * sizeof(raft_ent_t)); }
int main(void) {
    raft_t a, b, c; raft_disk_t d; int k;
    printf("== 1. elections ==\n");
    raft_init(&a, 0, 3, 0, 0); check("a new node is a follower in term 0 with an empty log and no vote; its timeout is 10 + rnd % 10 ticks", a.role == RAFT_FOLLOWER && a.d.term == 0 && a.d.voted_for == -1 && a.d.log_len == 0 && a.timeout == 10);
    raft_init(&a, 0, 3, 0, 7); check("with random 7 the timeout is 17", a.timeout == 17);
    raft_init(&a, 0, 3, 0, 0); for (k = 0; k < 9; k++) { raft_tick(&a, 0, out); } check("nine ticks of a ten-tick timeout: still a follower", a.role == RAFT_FOLLOWER);
    k = raft_tick(&a, 0, out); check("the tenth starts an election: term 1, a vote for itself, role candidate, a RequestVote for each of the two peers", k == 2 && a.role == RAFT_CANDIDATE && a.d.term == 1 && a.d.voted_for == 0 && a.dirty && out[0].type == RAFT_MSG_RV && out[0].term == 1 && out[0].to == 1 && out[1].to == 2 && out[0].last_idx == 0 && out[0].last_term == 0);
    raft_init(&b, 1, 3, 0, 0); raft_msg_t m = rv(0, 1, 1, 0, 0); k = raft_recv(&b, &m, 0, out);
    check("a follower grants the vote: reply granted, term 1, votedFor 0, persistent state marked dirty", k == 1 && out[0].type == RAFT_MSG_RVR && out[0].granted == 1 && out[0].term == 1 && b.d.voted_for == 0 && b.d.term == 1 && b.dirty);
    raft_msg_t reply = out[0]; k = raft_recv(&a, &reply, 0, out);
    check("one granted vote plus its own is a majority of three: the candidate becomes leader, appends a no-op (command 0, term 1) and sends AppendEntries to both peers", k == 2 && a.role == RAFT_LEADER && a.d.log_len == 1 && a.d.log[0].term == 1 && a.d.log[0].cmd == 0 && out[0].type == RAFT_MSG_AE && out[0].nent == 1 && out[0].prev_idx == 0);
    raft_init(&c, 2, 3, 0, 0); m = rv(0, 2, 1, 0, 0); raft_recv(&c, &m, 0, out); m = rv(1, 2, 1, 0, 0); k = raft_recv(&c, &m, 0, out);
    check("a second candidate in the same term is refused: one vote per term", k == 1 && out[0].granted == 0 && c.d.voted_for == 0);
    m = rv(0, 2, 1, 0, 0); k = raft_recv(&c, &m, 0, out); check("the same candidate asking again (a duplicated message) is granted again, idempotently", out[0].granted == 1);
    m = rv(1, 2, 0, 0, 0); k = raft_recv(&c, &m, 0, out); check("a RequestVote from an older term is refused and answered with the current term", out[0].granted == 0 && out[0].term == 1);
    raft_init(&b, 1, 3, 0, 0); b.d.term = 3; m = rv(0, 1, 2, 0, 0); raft_recv(&b, &m, 0, out); check("a RequestVote from an older term is refused EVEN WHEN the node has not voted yet in its own term, and the node does not record a vote for that stale candidate", out[0].granted == 0 && b.d.voted_for == -1 && out[0].term == 3);
    { uint32_t t[3] = {1, 1, 2}; log_of(&d, 2, -1, 3, t); raft_init(&b, 1, 5, &d, 0);
      m = rv(0, 1, 3, 3, 1); raft_recv(&b, &m, 0, out); check("the up-to-date rule: a candidate whose last log TERM is lower is refused even with a longer log (last term 1 < 2)", out[0].granted == 0 && b.d.term == 3);
      raft_init(&b, 1, 5, &d, 0); m = rv(0, 1, 3, 2, 2); raft_recv(&b, &m, 0, out); check("equal last term but a shorter log (2 < 3) is refused", out[0].granted == 0);
      raft_init(&b, 1, 5, &d, 0); m = rv(0, 1, 3, 3, 2); raft_recv(&b, &m, 0, out); check("equal last term and equal length is granted", out[0].granted == 1);
      raft_init(&b, 1, 5, &d, 0); m = rv(0, 1, 3, 1, 3); raft_recv(&b, &m, 0, out); check("a higher last term wins even with a shorter log", out[0].granted == 1); }
    printf("== 2. terms ==\n");
    raft_init(&a, 0, 3, 0, 0); a.role = RAFT_LEADER; a.d.term = 5; m = ae(1, 0, 6, 0, 0, 0, 0, 0); raft_recv(&a, &m, 0, out); check("a leader that sees a higher term steps down and adopts it (and forgets its vote)", a.role == RAFT_FOLLOWER && a.d.term == 6 && a.d.voted_for == -1);
    raft_init(&b, 1, 3, 0, 0); b.d.term = 4; m = ae(0, 1, 3, 0, 0, 0, 0, 0); raft_recv(&b, &m, 0, out); check("an AppendEntries from an older term is refused and does not reset the election timer", out[0].success == 0 && out[0].term == 4 && b.leader == -1);
    raft_init(&b, 1, 3, 0, 0); b.role = RAFT_CANDIDATE; b.d.term = 2; m = ae(0, 1, 2, 0, 0, 0, 0, 0); raft_recv(&b, &m, 0, out); check("a candidate that receives an AppendEntries of its own term yields to that leader", b.role == RAFT_FOLLOWER && b.leader == 0 && out[0].success == 1);
    printf("== 3. replication: the consistency check, conflicts, commit ==\n");
    { uint32_t t[3] = {1, 1, 2}; log_of(&d, 2, -1, 3, t); raft_init(&b, 1, 3, &d, 0); raft_ent_t e[1] = {{3, 7}};
      m = ae(0, 1, 3, 4, 3, 0, 1, e); raft_recv(&b, &m, 0, out); check("a prev_idx beyond the follower's log is refused (the leader will back up)", out[0].success == 0 && b.d.log_len == 3);
      m = ae(0, 1, 3, 3, 1, 0, 1, e); raft_recv(&b, &m, 0, out); check("a prev_term that does not match is refused", out[0].success == 0 && b.d.log_len == 3);
      m = ae(0, 1, 3, 3, 2, 0, 1, e); raft_recv(&b, &m, 0, out); check("a matching prev entry: the entry is appended, the reply says success with match 4", out[0].success == 1 && out[0].match == 4 && b.d.log_len == 4 && b.d.log[3].term == 3 && b.d.log[3].cmd == 7);
      m = ae(0, 1, 3, 3, 2, 0, 1, e); raft_recv(&b, &m, 0, out); check("the same message delivered again (a duplicate) changes nothing", out[0].success == 1 && b.d.log_len == 4);
      raft_ent_t old[1] = {{2, 100}}; m = ae(0, 1, 3, 2, 1, 0, 1, old); raft_recv(&b, &m, 0, out); check("a delayed message that carries an entry the follower already has does NOT truncate the longer log that follows it", out[0].success == 1 && b.d.log_len == 4); }
    { uint32_t t[4] = {1, 1, 2, 2}; log_of(&d, 2, -1, 4, t); raft_init(&b, 1, 3, &d, 0); raft_ent_t e[2] = {{3, 8}, {3, 9}}; m = ae(0, 1, 3, 2, 1, 0, 2, e); raft_recv(&b, &m, 0, out);
      check("a conflicting entry (index 3: term 2 held, term 3 sent) deletes it and everything after it, then appends the leader's", out[0].success == 1 && b.d.log_len == 4 && b.d.log[2].term == 3 && b.d.log[3].term == 3 && b.d.log[3].cmd == 9); }
    { uint32_t t[2] = {1, 1}; log_of(&d, 1, -1, 2, t); raft_init(&b, 1, 3, &d, 0); m = ae(0, 1, 1, 2, 1, 5, 0, 0); raft_recv(&b, &m, 0, out);
      check("commitIndex follows min(leaderCommit, index of the last new entry): leaderCommit 5 on a log of 2 gives 2", b.commit == 2);
      raft_ent_t e[2] = {{1, 5}, {1, 6}}; m = ae(0, 1, 1, 2, 1, 3, 2, e); raft_recv(&b, &m, 0, out); check("leaderCommit 3 with entries up to 4 gives 3, and the entries are applied in order", b.commit == 3 && b.applied == 3);
      m = ae(0, 1, 1, 0, 0, 4, 1, (raft_ent_t[1]){{1, 100}}); raft_recv(&b, &m, 0, out); check("a delayed message with a larger leaderCommit but fewer entries (min = 1) does not move commitIndex BACKWARDS (a bug the simulator found in this chapter's first version)", b.commit == 3); }
    printf("== 4. the leader: matchIndex, commit, backing up ==\n");
    { uint32_t t[2] = {1, 2}; log_of(&d, 2, 0, 2, t); raft_init(&a, 0, 3, &d, 0); a.role = RAFT_LEADER; for (k = 0; k < 3; k++) { a.next[k] = 3; a.match[k] = 0; } a.match[0] = 2;
      raft_msg_t r; memset(&r, 0, sizeof r); r.type = RAFT_MSG_AER; r.from = 1; r.to = 0; r.term = 2; r.success = 0; k = raft_recv(&a, &r, 0, out);
      check("a refused AppendEntries makes the leader back up one entry and resend at once (prev_idx 1)", k == 1 && a.next[1] == 2 && out[0].type == RAFT_MSG_AE && out[0].to == 1 && out[0].prev_idx == 1 && out[0].nent == 1);
      r.success = 1; r.match = 2; raft_recv(&a, &r, 0, out); check("a successful reply with match 2 from one follower: a majority of three holds index 2 (term 2 = the leader's term), so it is committed", a.commit == 2 && a.match[1] == 2 && a.next[1] == 3 && a.applied == 2);
      r.match = 1; raft_recv(&a, &r, 0, out); check("a delayed success reply with a smaller match never lowers matchIndex", a.match[1] == 2); }
    printf("== 5. Figure 8 of the Raft paper: an entry of an earlier term is never committed by counting replicas ==\n");
    { uint32_t t[2] = {1, 2}; log_of(&d, 4, 0, 2, t); raft_init(&a, 0, 5, &d, 0); a.role = RAFT_LEADER; for (k = 0; k < 5; k++) { a.next[k] = 3; a.match[k] = 0; } a.match[0] = 2; /* S1 is leader of term 4 holding index 2 from term 2, as in Figure 8 (c) */
      raft_msg_t r; memset(&r, 0, sizeof r); r.type = RAFT_MSG_AER; r.from = 1; r.to = 0; r.term = 4; r.success = 1; r.match = 2; raft_recv(&a, &r, 0, out); r.from = 2; raft_recv(&a, &r, 0, out);
      check("three of five servers (itself and two followers) now hold index 2, which is from term 2: it must NOT be committed (a later leader with a term-3 entry could still overwrite it)", a.commit == 0);
      r.from = 3; raft_recv(&a, &r, 0, out); check("not even a fourth replica commits it", a.commit == 0);
      uint32_t idx; raft_propose(&a, 55, &idx); check("the leader appends an entry of its own term 4 at index 3", idx == 3 && a.d.log[2].term == 4);
      r.from = 1; r.match = 3; raft_recv(&a, &r, 0, out); check("one follower holds index 3: two of five, not a majority: still nothing committed", a.commit == 0);
      r.from = 2; raft_recv(&a, &r, 0, out); check("a second follower holds index 3 (three of five, term 4): index 3 is committed, and with it, indirectly, index 2", a.commit == 3 && a.applied == 3); }
    printf("== 6. Figure 7 of the Raft paper: a leader brings six very different logs into line ==\n");
    { static const uint32_t L[10] = {1, 1, 1, 4, 4, 5, 5, 6, 6, 6}; static const uint32_t A[9] = {1, 1, 1, 4, 4, 5, 5, 6, 6}, B[4] = {1, 1, 1, 4}, C[11] = {1, 1, 1, 4, 4, 5, 5, 6, 6, 6, 6}, D[12] = {1, 1, 1, 4, 4, 5, 5, 6, 6, 6, 7, 7}, E[7] = {1, 1, 1, 4, 4, 4, 4}, F[11] = {1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3};
      static raft_t nodes[7]; const uint32_t *lg[7] = {L, A, B, C, D, E, F}; int ln[7] = {10, 9, 4, 11, 12, 7, 11}; for (k = 0; k < 7; k++) { log_of(&d, k == 0 ? 8 : (k == 4 ? 7 : 6), -1, ln[k], lg[k]); raft_init(&nodes[k], k, 7, &d, 0); }
      raft_t *ld = &nodes[0]; ld->role = RAFT_LEADER; ld->d.term = 8; for (k = 0; k < 7; k++) { ld->next[k] = 11; ld->match[k] = 0; } ld->match[0] = 10; uint32_t idx; raft_propose(ld, 77, &idx);
      raft_msg_t q[400]; int nq = 0;
      /* a heartbeat round, then deliver every message and every reply until the system is quiet */
      for (int round = 0; round < 40; round++) { int c2 = raft_tick(ld, 0, out); ld->hb = RAFT_HEARTBEAT; c2 = raft_tick(ld, 0, out); for (int i = 0; i < c2; i++) { q[nq++] = out[i]; } int guard = 0;
          while (nq > 0 && guard++ < 400) { raft_msg_t x = q[--nq]; raft_msg_t o2[RAFT_MAX_NODES]; int cc = raft_recv(&nodes[x.to], &x, 0, o2); for (int i = 0; i < cc; i++) { q[nq++] = o2[i]; } } }
      int all = 1; for (k = 1; k < 7; k++) { if (!same_log(&nodes[k], ld)) { all = 0; } }
      check("after the leader (term 8, 11 entries including a new one) has talked to them, all six followers' logs are identical to the leader's: (a) was missing 2 entries, (b) 7, (c) and (d) had extra uncommitted entries from terms 6 and 7, (e) a different term 4 tail, (f) a completely different history", all && ld->d.log_len == 11);
      check("the leader's own log was never changed by the exchange", ld->d.log_len == 11 && ld->d.log[3].term == 4 && ld->d.log[9].term == 6 && ld->d.log[10].term == 8);
      check("and the entries are committed everywhere once a majority holds the term-8 entry", ld->commit == 11 && nodes[3].commit >= 10); }
    printf("== 7. persistence, proposals, the state machine ==\n");
    raft_init(&a, 0, 3, 0, 0); a.d.voted_for = 1; raft_disk_t saved = a.d; a.dirty = 0; raft_init(&b, 0, 3, &saved, 0); check("a node restarted from its saved state keeps its vote, term and log (and has no volatile state: commit 0, follower)", b.d.voted_for == 1 && b.commit == 0 && b.role == RAFT_FOLLOWER);
    raft_init(&a, 0, 3, 0, 0); uint32_t idx = 0; check("a follower refuses a client command: RAFT_ERR_NOT_LEADER", raft_propose(&a, 5, &idx) == RAFT_ERR_NOT_LEADER);
    a.role = RAFT_LEADER; a.d.term = 1; check("command 0 is reserved for the no-op and refused", raft_propose(&a, 0, &idx) == RAFT_ERR_FULL);
    check("a leader accepts a command: index 1, term 1, persistent state marked dirty", raft_propose(&a, 5, &idx) == RAFT_OK && idx == 1 && a.d.log[0].term == 1 && a.d.log[0].cmd == 5 && a.dirty);
    for (k = 1; k < RAFT_LOG_MAX; k++) { raft_propose(&a, 6, &idx); } check("the 65th command is refused when the log holds 64 entries: RAFT_ERR_FULL", raft_propose(&a, 7, &idx) == RAFT_ERR_FULL && a.d.log_len == 64);
    { uint32_t t[3] = {1, 1, 1}; log_of(&d, 1, -1, 3, t); raft_init(&b, 1, 3, &d, 0); m = ae(0, 1, 1, 3, 1, 3, 0, 0); raft_recv(&b, &m, 0, out); uint32_t h = 2166136261u; for (int i = 0; i < 3; i++) { h = (h ^ (100u + (uint32_t)i)) * 16777619u; }
      check("committed entries are applied in order: three commands counted and hashed as FNV-1a over their values (computed here independently)", b.applied == 3 && b.sm_count == 3 && b.sm_hash == h); }
    { uint32_t t[1] = {1}; log_of(&d, 1, -1, 1, t); d.log[0].cmd = 0; raft_init(&b, 1, 3, &d, 0); m = ae(0, 1, 1, 1, 1, 1, 0, 0); raft_recv(&b, &m, 0, out); check("a no-op is applied but not counted", b.applied == 1 && b.sm_count == 0); }
    raft_init(&a, 0, 3, 0, 0); a.role = RAFT_LEADER; a.d.term = 1; for (k = 0; k < 3; k++) { a.next[k] = 1; } { int hb = 0; for (k = 0; k < 9; k++) { hb += raft_tick(&a, 0, out) > 0; } check("a leader sends a heartbeat every 3 ticks: 3 in 9 ticks", hb == 3); }
    printf("\n%d passed, %d failed\n", pass_n, fail_n); return fail_n ? 1 : 0;
}
