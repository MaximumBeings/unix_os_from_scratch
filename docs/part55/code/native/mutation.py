#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the suite to FAIL every time. A "caught" line is the EXPECTED, wanted
result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified. For each mutant it runs: raft_test (worked by hand, with Figures 7 and 8 of the paper), the differential test against the independent Python node and simulator (the correct node and six deliberate bugs, every seed's trace hash compared) and a 400-seed run of the simulator against the node. Four broken copies are tested at a time. Output: mutation_out.txt   Usage: mutation.py"""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
MUT = [
 ('055_raft.c', 'vote: granted to a candidate of an older term', 'if (m->term == r->d.term && (r->d.voted_for == -1 ||', 'if (m->term >= r->d.term && (r->d.voted_for == -1 ||'),
 ('055_raft.c', 'vote: a second vote in the same term is allowed', '(r->d.voted_for == -1 || r->d.voted_for == m->from)) {\n            uint32_t lt', '(1)) {\n            uint32_t lt'),
 ('055_raft.c', 'vote: the log rule compares last term the wrong way', 'int up_to_date = m->last_term > lt ||', 'int up_to_date = m->last_term < lt ||'),
 ('055_raft.c', 'vote: equal last terms need a strictly longer log', '(m->last_term == lt && m->last_idx >= li);', '(m->last_term == lt && m->last_idx > li);'),
 ('055_raft.c', 'vote: equal last terms accept any length', '(m->last_term == lt && m->last_idx >= li);', '(m->last_term == lt);'),
 ('055_raft.c', 'vote: the vote is not recorded', 'r->d.voted_for = m->from; r->dirty = 1; o->granted = 1;', 'o->granted = 1;'),
 ('055_raft.c', 'vote: the reply carries the term from before', 'o->term = r->d.term; return 1;', 'return 1;'),
 ('055_raft.c', 'vote: a granted vote does not reset the election timer', 'o->granted = 1; reset_timer(r, rnd); }', 'o->granted = 1; }'),
 ('055_raft.c', 'candidate: the vote for itself is not recorded', 'r->d.term++; r->d.voted_for = r->id; r->dirty = 1;', 'r->d.term++; r->d.voted_for = -1; r->dirty = 1;'),
 ('055_raft.c', 'candidate: the term is not incremented', 'r->d.term++; r->d.voted_for = r->id;', 'r->d.voted_for = r->id;'),
 ('055_raft.c', 'candidate: its own vote is not counted', 'r->role = RAFT_CANDIDATE; r->votes = 1u << r->id;', 'r->role = RAFT_CANDIDATE; r->votes = 0;'),
 ('055_raft.c', 'candidate: it asks with its first log term instead of its last', 'm->last_term = term_at(r, last_index(r)); }', 'm->last_term = term_at(r, 1); }'),
 ('055_raft.c', 'leader: a minority of votes wins the election', 'if (popcount(r->votes) <= r->n / 2) { return 0; }', 'if (popcount(r->votes) < r->n / 2) { return 0; }'),
 ('055_raft.c', 'leader: votes of other terms are counted', 'if (r->role != RAFT_CANDIDATE || m->term != r->d.term || !m->granted) { return 0; }', 'if (r->role != RAFT_CANDIDATE || !m->granted) { return 0; }'),
 ('055_raft.c', 'leader: refused votes are counted', '|| m->term != r->d.term || !m->granted) { return 0; }', '|| m->term != r->d.term) { return 0; }'),
 ('055_raft.c', 'leader: nextIndex starts at the end of its log, not past it', 'r->next[i] = last_index(r) + 1; r->match[i] = 0; }', 'r->next[i] = last_index(r); r->match[i] = 0; }'),
 ('055_raft.c', 'leader: no no-op is appended', 'if (last_index(r) < RAFT_LOG_MAX) { r->d.log[r->d.log_len].term = r->d.term; r->d.log[r->d.log_len].cmd = 0;', 'if (0) { r->d.log[r->d.log_len].term = r->d.term; r->d.log[r->d.log_len].cmd = 0;'),
 ('055_raft.c', 'leader: the no-op carries the previous term', 'r->d.log[r->d.log_len].term = r->d.term; r->d.log[r->d.log_len].cmd = 0; r->d.log_len++;', 'r->d.log[r->d.log_len].term = r->d.term - 1; r->d.log[r->d.log_len].cmd = 0; r->d.log_len++;'),
 ('055_raft.c', 'append: a stale AppendEntries (older term) is accepted', 'if (m->term < r->d.term) { return 1; }\n        r->role = RAFT_FOLLOWER;', 'r->role = RAFT_FOLLOWER;'),
 ('055_raft.c', 'append: the heartbeat does not reset the election timer', 'r->role = RAFT_FOLLOWER; r->leader = m->from; reset_timer(r, rnd);', 'r->role = RAFT_FOLLOWER; r->leader = m->from;'),
 ('055_raft.c', "append: the prev entry's term is not compared", '(m->prev_idx > 0 && term_at(r, m->prev_idx) != m->prev_term))) { return 1; }', '(0))) { return 1; }'),
 ('055_raft.c', 'append: a prev index beyond the log is accepted', 'if (raft_bug != RAFT_BUG_NO_CONSISTENCY_CHECK && (m->prev_idx > last_index(r) ||', 'if (raft_bug != RAFT_BUG_NO_CONSISTENCY_CHECK && (0 ||'),
 ('055_raft.c', 'append: a conflicting entry is not deleted', 'if (idx <= last_index(r) && r->d.log[idx - 1].term != m->ent[k].term) { r->d.log_len = idx - 1; r->dirty = 1; }', 'if (0) { r->d.log_len = idx - 1; r->dirty = 1; }'),
 ('055_raft.c', 'append: every overlapping entry is deleted, even a matching one', 'if (idx <= last_index(r) && r->d.log[idx - 1].term != m->ent[k].term) { r->d.log_len = idx - 1;', 'if (idx <= last_index(r)) { r->d.log_len = idx - 1;'),
 ('055_raft.c', 'append: a conflict keeps the conflicting entry', 'r->d.log_len = idx - 1; r->dirty = 1; } /* a conflicting entry', 'r->d.log_len = idx; r->dirty = 1; } /* a conflicting entry'),
 ('055_raft.c', 'append: the log is not marked dirty after an append', 'r->d.log[r->d.log_len++] = m->ent[k]; r->dirty = 1; }', 'r->d.log[r->d.log_len++] = m->ent[k]; }'),
 ('055_raft.c', 'append: commitIndex takes leaderCommit even beyond the new entries', 'uint32_t nc = m->commit < mi ? m->commit : mi;', 'uint32_t nc = m->commit;'),
 ('055_raft.c', 'append: commitIndex may move backwards', 'if (nc > r->commit || raft_bug == RAFT_BUG_COMMIT_REGRESS) { r->commit = nc;', 'if (1) { r->commit = nc;'),
 ('055_raft.c', 'append: the success reply reports the end of the log, not of the new entries', 'o->success = 1; o->match = mi; return 1;', 'o->success = 1; o->match = last_index(r); return 1;'),
 ('055_raft.c', 'commit: a minority commits', 'if (raft_bug == RAFT_BUG_MINORITY_COMMIT ? c >= r->n / 2 : c > r->n / 2) { r->commit = idx; break; }', 'if (c >= r->n / 2) { r->commit = idx; break; }'),
 ('055_raft.c', 'commit: entries of earlier terms are committed by counting replicas', 'int same_term = r->d.log[idx - 1].term == r->d.term || raft_bug == RAFT_BUG_COMMIT_OLD_TERM;', 'int same_term = 1;'),
 ('055_raft.c', 'commit: a follower at exactly the index is not counted', 'if (i != r->id && r->match[i] >= idx) { c++; }', 'if (i != r->id && r->match[i] > idx) { c++; }'),
 ('055_raft.c', "commit: the leader's own copy is not counted", 'if (!same_term) { continue; } int c = 1;', 'if (!same_term) { continue; } int c = 0;'),
 ('055_raft.c', 'commit: the search starts below the end of the log', 'for (uint32_t idx = last_index(r); idx > r->commit; idx--) {', 'for (uint32_t idx = last_index(r) - 1; idx > r->commit; idx--) {'),
 ('055_raft.c', 'reply: matchIndex may go down on a delayed success reply', 'if (m->success) { if (m->match > r->match[m->from]) { r->match[m->from] = m->match; }', 'if (m->success) { r->match[m->from] = m->match;'),
 ('055_raft.c', 'reply: nextIndex is not advanced after a success', 'r->next[m->from] = r->match[m->from] + 1; advance_commit(r); return 0; }', 'advance_commit(r); return 0; }'),
 ('055_raft.c', 'reply: a failed AppendEntries does not back up', 'if (r->next[m->from] > 1) { r->next[m->from]--; } make_ae(r, m->from, &out[0]); return 1;', 'make_ae(r, m->from, &out[0]); return 1;'),
 ('055_raft.c', 'reply: it backs up to the beginning of the log', 'if (r->next[m->from] > 1) { r->next[m->from]--; } make_ae', 'r->next[m->from] = 1; make_ae'),
 ('055_raft.c', 'reply: a failed reply is not answered with a resend', 'make_ae(r, m->from, &out[0]); return 1; /* back up one entry and try again at once */', 'return 0;'),
 ('055_raft.c', 'reply: replies from an older term are acted on', 'if (r->role != RAFT_LEADER || m->term != r->d.term) { return 0; }\n        if (m->success)', 'if (r->role != RAFT_LEADER) { return 0; }\n        if (m->success)'),
 ('055_raft.c', 'terms: a message of the SAME term resets the vote', 'static void become_follower(raft_t *r, uint32_t term) { if (term > r->d.term) {', 'static void become_follower(raft_t *r, uint32_t term) { if (term >= r->d.term) {'),
 ('055_raft.c', 'terms: a higher term does not clear the vote', 'r->d.term = term; r->d.voted_for = -1; r->dirty = 1; }', 'r->d.term = term; r->dirty = 1; }'),
 ('055_raft.c', 'terms: a higher term is not saved', 'if (term > r->d.term) { r->d.term = term; r->d.voted_for = -1; r->dirty = 1; }', 'if (term > r->d.term) { r->d.term = term; r->d.voted_for = -1; }'),
 ('055_raft.c', 'terms: a leader does not step down on a higher term', 'if (m->term > r->d.term) { become_follower(r, m->term); }', 'if (m->term > r->d.term && r->role != RAFT_LEADER) { become_follower(r, m->term); }'),
 ('055_raft.c', 'timers: the heartbeat comes every 4 ticks', 'if (r->hb >= RAFT_HEARTBEAT) {', 'if (r->hb > RAFT_HEARTBEAT) {'),
 ('055_raft.c', 'timers: the election timeout is one tick longer', 'if (r->elapsed < r->timeout) { return 0; }', 'if (r->elapsed <= r->timeout) { return 0; }'),
 ('055_raft.c', 'timers: the random part of the timeout is ignored', 'r->timeout = RAFT_ELECT_MIN + rnd % RAFT_ELECT_RANGE;', 'r->timeout = RAFT_ELECT_MIN;'),
 ('055_raft.c', 'state machine: no-ops are counted', 'if (c != 0) { r->sm_count++;', '{ r->sm_count++;'),
 ('055_raft.c', 'state machine: the hash ignores the command', 'r->sm_hash = (r->sm_hash ^ c) * 16777619u;', 'r->sm_hash = (r->sm_hash ^ 1u) * 16777619u;'),
 ('055_raft.c', 'state machine: entries are applied beyond the commit index', 'while (r->applied < r->commit && r->applied < r->d.log_len) {', 'while (r->applied < r->d.log_len) {'),
 ('055_raft.c', 'propose: command 0 is accepted', 'if (cmd == 0 || r->d.log_len >= RAFT_LOG_MAX) { return RAFT_ERR_FULL; }', 'if (r->d.log_len >= RAFT_LOG_MAX) { return RAFT_ERR_FULL; }'),
 ('055_raft.c', 'propose: a full log is not refused (buffer overrun)', 'if (cmd == 0 || r->d.log_len >= RAFT_LOG_MAX) { return RAFT_ERR_FULL; }', 'if (cmd == 0) { return RAFT_ERR_FULL; }'),
 ('055_raft.c', 'propose: the new entry is not marked dirty', 'r->d.log[r->d.log_len].cmd = cmd; r->d.log_len++; r->dirty = 1;', 'r->d.log[r->d.log_len].cmd = cmd; r->d.log_len++;'),
 ('055_raft.c', 'propose: a follower may propose', 'if (r->role != RAFT_LEADER) { return RAFT_ERR_NOT_LEADER; }', ''),
 ('055_raft.c', 'messages: an AppendEntries carries 5 entries (buffer overrun)', 'if (cnt > RAFT_MAX_ENT) { cnt = RAFT_MAX_ENT; }', 'if (cnt > RAFT_MAX_ENT + 1) { cnt = RAFT_MAX_ENT + 1; }'),
 ('055_raft.c', 'messages: the prev term is sent as 0', 'm->prev_term = term_at(r, prev);', 'm->prev_term = 0;'),
 ('055_raft.c', "messages: the leader's commit index is not sent", 'm->commit = r->commit; m->nent = cnt;', 'm->commit = 0; m->nent = cnt;'),
 ('055_raft.c', 'restart: the log is not restored', 'if (disk) { r->d = *disk; if (raft_bug', 'if (disk) { r->d.term = disk->term; r->d.voted_for = disk->voted_for; if (raft_bug'),
 ('055_raft.c', 'restart: the term is not restored', 'if (disk) { r->d = *disk; if (raft_bug', 'if (disk) { r->d = *disk; r->d.term = 0; if (raft_bug'),
 ('055_sim.c', 'simulator: the election-safety check is removed', 'else if (s->leader_of_term[r->d.term] != i + 1) { viol(s, 1, i); } } }', 'else if (0) { viol(s, 1, i); } } }'),
 ('055_sim.c', 'simulator: the log-matching check is removed', 'a->d.log[q - 1].cmd != b->d.log[q - 1].cmd) { viol(s, 2, i); }', 'a->d.log[q - 1].cmd != b->d.log[q - 1].cmd) { }'),
 ('055_sim.c', 'simulator: the committed-entry comparison is removed', '|| r->d.log[k - 1].cmd != s->gc[k - 1].cmd) { viol(s, 3, i); } }\n        while', '|| 0) { } }\n        while'),
 ('055_sim.c', 'simulator: leader completeness is not checked', 'if (r->d.term > s->gc[k - 1].ct && (r->d.log_len < k ||', 'if (0 && r->d.term > s->gc[k - 1].ct && (r->d.log_len < k ||'),
 ('055_sim.c', 'simulator: commit monotonicity is not checked', 'if (r->commit < s->last_commit[i]) { viol(s, 6, i); }', 'if (0) { viol(s, 6, i); }'),
 ('055_sim.c', 'simulator: the disk is not saved after a delivery', 'int c = raft_recv(&s->node[m.to], &m, rr, out); save(s, m.to);', 'int c = raft_recv(&s->node[m.to], &m, rr, out);'),
 ('055_sim.c', 'simulator: partitions do not cut links', 'return !s->partitioned || s->group[a] == s->group[b];', 'return 1;'),
 ('055_sim.c', 'simulator: a restart starts from an empty disk', 'start_node(s, i, 0); s->restarts++;', 'start_node(s, i, 1); s->restarts++;'),
 ('055_sim.c', 'simulator: duplicates are never made', 'if (rnd(s) % 1000 < s->cfg.dup_pm) { copies = 2; s->dups++; }', 'if (rnd(s) % 1000 < s->cfg.dup_pm && 0) { copies = 2; s->dups++; }'),
 ('raft_ref.py', 'reference: the log rule is skipped for every vote even when no bug is switched on (the oracle itself broken)', 'if bug == 1: ok = True', 'ok = True'),
]
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, errors="replace", **kw)
SRC = ["../055_raft.c", "../055_sim.c"]; G = ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
def suite(tmp):
    failed = []; n = os.path.join(tmp, "native")
    b = run(n, G + ["raft_test.c", "../055_raft.c", "-o", "rt"])
    if b.returncode != 0: failed.append("raft_test (did not build)")
    elif run(n, ["./rt"]).returncode != 0: failed.append("raft_test")
    b = run(n, G + ["raft_cli.c"] + SRC + ["-o", "cli"])
    if b.returncode != 0: failed.append("the simulator (did not build)"); return failed
    if run(n, [sys.executable, "diff_raft.py", "60", "./cli"]).returncode != 0: failed.append("simulator vs the Python node and simulator")
    b = run(n, G + ["raft_sweep.c"] + SRC + ["-o", "sw"])
    if b.returncode != 0: failed.append("sweep (did not build)")
    elif run(n, ["./sw", "5000", "400"]).returncode != 0: failed.append("sweep")
    return failed
print("NOTE: this script deliberately breaks copies of the chapter's sources. 'caught' lines are EXPECTED: they show the tests can detect the mistake."); sys.stdout.flush()
base = tempfile.mkdtemp(prefix="c51mut_"); shutil.copytree(code, os.path.join(base, "base"), ignore=shutil.ignore_patterns("build", "*.o", "__pycache__"))
f0 = suite(os.path.join(base, "base")); print("baseline (nothing broken):", "all tests pass (expected)" if not f0 else "UNEXPECTED FAILURES " + str(f0)); sys.stdout.flush()
def one(i):
    fn, label, old, new = MUT[i]; tmp = os.path.join(base, "m%d" % i); shutil.copytree(os.path.join(base, "base"), tmp)
    p = os.path.join(tmp, fn); s = open(p, encoding="utf-8").read(); assert s.count(old) == 1, (label, s.count(old)); open(p, "w", encoding="utf-8").write(s.replace(old, new))
    failed = suite(tmp); shutil.rmtree(tmp, ignore_errors=True); return label, failed
caught = 0
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    for label, failed in pool.map(one, range(len(MUT))):
        if failed: caught += 1; print(f"{label}: caught by {', '.join(failed)}")
        else: print(f"{label}: NOT CAUGHT")
        sys.stdout.flush()
print(f"\n{caught} of {len(MUT)} broken versions caught"); shutil.rmtree(base, ignore_errors=True)
