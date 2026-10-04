#!/usr/bin/env python3
"""Chapter 55: an independent second implementation of the Raft node AND of the deterministic simulator, in Python, written from the Raft paper (Figure 2 and Figure 3) and from the simulator's specification in 055_sim.h, not from the C code. Given the same seed it
must reproduce the C simulator's run exactly: the same statistics, the same violation (if a bug is switched on), and the same TRACE HASH, a running hash of every delivered message and of every node's state after every tick. If the two ever diverged by one message, the hashes would differ.
Usage: raft_ref.py FIRST_SEED LAST_SEED [BUG [TICKS]]   (same output format as native/raft_cli.c)"""
import sys
M32 = 0xFFFFFFFF
FOLLOWER, CANDIDATE, LEADER = 0, 1, 2
RV, RVR, AE, AER = 1, 2, 3, 4
MAXLOG, MAXENT, EMIN, ERANGE, HB = 64, 4, 10, 10, 3
bug = 0
class Msg:
    __slots__ = ("type", "frm", "to", "term", "last_idx", "last_term", "granted", "prev_idx", "prev_term", "commit", "nent", "ent", "success", "match")
    def __init__(s, **kw):
        s.type = s.frm = s.to = s.term = s.last_idx = s.last_term = s.granted = s.prev_idx = s.prev_term = s.commit = s.nent = s.success = s.match = 0; s.ent = []
        for k, v in kw.items(): setattr(s, k, v)
class Node:
    def __init__(s, id, n, disk, rnd):
        s.id, s.n, s.role, s.leader, s.dirty = id, n, FOLLOWER, -1, False
        s.term, s.voted, s.log = 0, -1, []
        if disk:
            s.term, s.voted, s.log = disk[0], disk[1], list(disk[2])
            if bug == 3: s.voted = -1
        s.commit = s.applied = s.elapsed = s.hb = s.votes = 0; s.next = [0] * n; s.match = [0] * n; s.sm_count = 0; s.sm_hash = 2166136261
        s.reset(rnd)
    def disk(s): return (s.term, s.voted, tuple(s.log))
    def reset(s, rnd): s.elapsed = 0; s.timeout = EMIN + rnd % ERANGE
    def last(s): return len(s.log)
    def term_at(s, i): return 0 if i == 0 else s.log[i - 1][0]
    def apply(s):
        while s.applied < s.commit and s.applied < len(s.log):
            s.applied += 1; c = s.log[s.applied - 1][1]
            if c != 0: s.sm_count += 1; s.sm_hash = ((s.sm_hash ^ c) * 16777619) & M32
    def follower(s, term):
        if term > s.term: s.term = term; s.voted = -1; s.dirty = True
        s.role = FOLLOWER; s.votes = 0
    def make_ae(s, to):
        nx = max(1, s.next[to]); prev = nx - 1; cnt = min(MAXENT, max(0, s.last() - nx + 1))
        return Msg(type=AE, frm=s.id, to=to, term=s.term, prev_idx=prev, prev_term=s.term_at(prev), commit=s.commit, nent=cnt, ent=[s.log[nx - 1 + i] for i in range(cnt)])
    def bcast(s): return [s.make_ae(i) for i in range(s.n) if i != s.id]
    def advance(s):
        for idx in range(s.last(), s.commit, -1):
            if not (s.log[idx - 1][0] == s.term or bug == 2): continue
            c = 1 + sum(1 for i in range(s.n) if i != s.id and s.match[i] >= idx)
            if (c >= s.n // 2 if bug == 5 else c > s.n // 2): s.commit = idx; break
        s.apply()
    def tick(s, rnd):
        s.elapsed += 1
        if s.role == LEADER:
            s.hb += 1
            if s.hb >= HB: s.hb = 0; return s.bcast()
            return []
        if s.elapsed < s.timeout: return []
        s.term += 1; s.voted = s.id; s.dirty = True; s.role = CANDIDATE; s.votes = 1 << s.id; s.leader = -1; s.reset(rnd)
        return [Msg(type=RV, frm=s.id, to=i, term=s.term, last_idx=s.last(), last_term=s.term_at(s.last())) for i in range(s.n) if i != s.id]
    def recv(s, m, rnd):
        if m.to != s.id or m.frm >= s.n or m.frm == s.id: return []
        if m.term > s.term: s.follower(m.term)
        if m.type == RV:
            o = Msg(type=RVR, frm=s.id, to=m.frm, term=s.term, granted=0)
            if m.term == s.term and (s.voted == -1 or s.voted == m.frm):
                lt, li = s.term_at(s.last()), s.last(); ok = m.last_term > lt or (m.last_term == lt and m.last_idx >= li)
                if bug == 1: ok = True
                if ok: s.voted = m.frm; s.dirty = True; o.granted = 1; s.reset(rnd)
            o.term = s.term; return [o]
        if m.type == RVR:
            if s.role != CANDIDATE or m.term != s.term or not m.granted: return []
            s.votes |= 1 << m.frm
            if bin(s.votes).count("1") <= s.n // 2: return []
            s.role = LEADER; s.leader = s.id; s.hb = 0; s.next = [s.last() + 1] * s.n; s.match = [0] * s.n
            if s.last() < MAXLOG: s.log.append((s.term, 0)); s.dirty = True; s.match[s.id] = s.last()
            return s.bcast()
        if m.type == AE:
            o = Msg(type=AER, frm=s.id, to=m.frm, term=s.term, success=0)
            if m.term < s.term: return [o]
            s.role = FOLLOWER; s.leader = m.frm; s.reset(rnd)
            if bug != 4 and (m.prev_idx > s.last() or (m.prev_idx > 0 and s.term_at(m.prev_idx) != m.prev_term)): return [o]
            for k in range(m.nent):
                idx = m.prev_idx + 1 + k
                if idx <= s.last() and s.log[idx - 1][0] != m.ent[k][0]: del s.log[idx - 1:]; s.dirty = True
                if idx > s.last():
                    if len(s.log) >= MAXLOG: return [o]
                    s.log.append(m.ent[k]); s.dirty = True
            mi = m.prev_idx + m.nent; nc = min(m.commit, mi)
            if nc > s.commit or bug == 6: s.commit = nc; s.apply()
            o.success = 1; o.match = mi; return [o]
        if m.type == AER:
            if s.role != LEADER or m.term != s.term: return []
            if m.success:
                if m.match > s.match[m.frm]: s.match[m.frm] = m.match
                s.next[m.frm] = s.match[m.frm] + 1; s.advance(); return []
            if s.next[m.frm] > 1: s.next[m.frm] -= 1
            return [s.make_ae(m.frm)]
        return []
    def propose(s, cmd):
        if s.role != LEADER: return None
        if cmd == 0 or len(s.log) >= MAXLOG: return None
        s.log.append((s.term, cmd)); s.dirty = True; s.match[s.id] = s.last(); return s.last()
def config(seed):
    n = (3, 5, 7)[(seed // 4) % 3]; c = dict(n=n, ticks=300, propose=80, drop=0, dup=0, delay=3, part=0, heal=60, crash=0, restart=0, target=0)
    p = seed % 4
    if p == 1: c.update(drop=200, dup=100, delay=9)
    elif p == 2: c.update(part=20, heal=50, delay=5)
    elif p == 3: c.update(drop=100, dup=60, delay=7, part=15, heal=50, crash=30, restart=80, target=70)
    return c
class Sim:
    def __init__(s, cfg, seed):
        s.cfg = cfg; n = cfg["n"]; s.rs = (seed * 2654435761 + 12345) & M32; s.trace = 2166136261; s.t = 0; s.next_cmd = 1
        s.disk = [(0, -1, ())] * n; s.alive = [True] * n; s.group = [0] * n; s.partitioned = False; s.q = []; s.gc = []; s.lot = {}; s.props = []
        s.last_commit = [0] * n; s.last_term = [0] * n; s.viol = 0; s.viol_tick = 0; s.viol_node = -1
        s.sent = s.delivered = s.dropped = s.dups = s.elections = s.crashes = s.restarts = s.proposals = s.acked = s.saves = s.max_commit = s.first_leader = 0
        s.node = [None] * n
        for i in range(n): s.node[i] = Node(i, n, None, s.rnd())
    def rnd(s): s.rs = (s.rs * 1664525 + 1013904223) & M32; return s.rs >> 8
    def mix(s, v):
        for sh in (0, 8, 16, 24): s.trace = ((s.trace ^ ((v >> sh) & 0xFF)) * 16777619) & M32
    def save(s, i):
        if s.node[i].dirty: s.disk[i] = s.node[i].disk(); s.node[i].dirty = False; s.saves += 1
    def push(s, m): delay = 1 + s.rnd() % s.cfg["delay"]; s.q.append((s.t + delay, m)) if len(s.q) < 256 else None
    def send(s, m):
        s.sent += 1
        if s.rnd() % 1000 < s.cfg["drop"]: s.dropped += 1; return
        copies = 1
        if s.rnd() % 1000 < s.cfg["dup"]: copies = 2; s.dups += 1
        for _ in range(copies): s.push(m)
    def linked(s, a, b): return (not s.partitioned) or s.group[a] == s.group[b]
    def v(s, code, node):
        if not s.viol: s.viol = code; s.viol_tick = s.t; s.viol_node = node
    def check(s):
        n = s.cfg["n"]; nodes = s.node; maxterm = 0
        for i in range(n): maxterm = max(maxterm, nodes[i].term if s.alive[i] else s.disk[i][0])
        for i in range(n):
            if not s.alive[i]: continue
            r = nodes[i]
            if r.term < s.last_term[i]: s.v(6, i)
            s.last_term[i] = r.term
            if r.commit < s.last_commit[i]: s.v(6, i)
            s.last_commit[i] = r.commit; s.max_commit = max(s.max_commit, r.commit)
            if r.role == LEADER and r.term < 256:
                if r.term not in s.lot:
                    s.lot[r.term] = i + 1; s.elections += 1
                    if not s.first_leader: s.first_leader = s.t
                elif s.lot[r.term] != i + 1: s.v(1, i)
        for i in range(n):
            for j in range(i + 1, n):
                if not (s.alive[i] and s.alive[j]): continue
                a, b = nodes[i].log, nodes[j].log
                for k in range(min(len(a), len(b)), 0, -1):
                    if a[k - 1][0] == b[k - 1][0]:
                        for q in range(k):
                            if a[q] != b[q]: s.v(2, i)
                        break
        for i in range(n):
            if not s.alive[i]: continue
            r = nodes[i]
            for k in range(1, min(r.commit, len(s.gc)) + 1):
                if k > len(r.log) or r.log[k - 1] != s.gc[k - 1][:2]: s.v(3, i)
            while len(s.gc) < r.commit and len(s.gc) < MAXLOG:
                if len(r.log) <= len(s.gc): s.v(3, i); break
                s.gc.append(r.log[len(s.gc)] + (maxterm,))
        for i in range(n):
            if not s.alive[i] or nodes[i].role != LEADER: continue
            r = nodes[i]
            for k in range(1, len(s.gc) + 1):
                if r.term > s.gc[k - 1][2] and (len(r.log) < k or r.log[k - 1] != s.gc[k - 1][:2]): s.v(4, i)
        for p in s.props:
            r = nodes[p["node"]]
            if not p["acked"] and s.alive[p["node"]] and r.role == LEADER and r.term == p["term"] and r.commit >= p["idx"] and r.log[p["idx"] - 1] == (p["term"], p["cmd"]): p["acked"] = True; s.acked += 1
            if p["acked"] and (len(s.gc) < p["idx"] or s.gc[p["idx"] - 1][1] != p["cmd"]): s.v(5, p["node"])
    def step(s):
        n = s.cfg["n"]; c = s.cfg; s.t += 1
        r = s.rnd()
        if not s.partitioned:
            if r % 1000 < c["part"]:
                s.partitioned = True
                for i in range(n): s.group[i] = s.rnd() % 3
        elif r % 1000 < c["heal"]: s.partitioned = False
        r = s.rnd()
        if r % 1000 < c["crash"]:
            i = s.rnd() % n
            if c["target"] and s.rnd() % 100 < c["target"]:
                for k in range(n):
                    if s.alive[k] and s.node[k].role == LEADER: i = k; break
            if s.alive[i]: s.alive[i] = False; s.crashes += 1
        r = s.rnd()
        if r % 1000 < c["restart"]:
            i = s.rnd() % n
            if not s.alive[i]: s.node[i] = Node(i, n, s.disk[i], s.rnd()); s.alive[i] = True; s.last_commit[i] = 0; s.restarts += 1
        r = s.rnd()
        if r % 1000 < c["propose"]:
            i = s.rnd() % n
            if s.alive[i] and s.node[i].role == LEADER and len(s.props) < 128:
                idx = s.node[i].propose(s.next_cmd)
                if idx is not None:
                    s.props.append(dict(idx=idx, term=s.node[i].term, cmd=s.next_cmd, node=i, acked=False)); s.next_cmd += 1; s.proposals += 1; s.save(i)
        q = list(s.q)
        for at, m in q:
            if at != s.t: continue
            rr = s.rnd()
            if s.alive[m.to] and s.linked(m.frm, m.to):
                s.delivered += 1; s.mix(s.t); s.mix(m.frm); s.mix(m.to); s.mix(m.type); s.mix(m.term); s.mix((m.nent + m.success + m.granted) & M32)
                outs = s.node[m.to].recv(m, rr); s.save(m.to)
                for o in outs: s.send(o)
        s.q = [(at, m) for at, m in s.q if at > s.t]
        for i in range(n):
            if not s.alive[i]: continue
            outs = s.node[i].tick(s.rnd()); s.save(i)
            for o in outs: s.send(o)
        for i in range(n):
            if s.alive[i]:
                for v in (i, s.node[i].role, s.node[i].term, len(s.node[i].log), s.node[i].commit): s.mix(v)
        s.check(); return s.viol
def run(seed, ticks=0):
    c = config(seed)
    if ticks: c["ticks"] = ticks
    s = Sim(c, seed)
    for _ in range(c["ticks"]):
        if s.viol: break
        s.step()
    return c, s
def line(seed, c, s): return f"seed {seed} n {c['n']} viol {s.viol} tick {s.viol_tick if s.viol else 0} node {s.viol_node if s.viol else -1} sent {s.sent} delivered {s.delivered} dropped {s.dropped} dups {s.dups} elections {s.elections} crashes {s.crashes} restarts {s.restarts} proposals {s.proposals} acked {s.acked} saves {s.saves} maxcommit {s.max_commit} firstleader {s.first_leader} trace {s.trace:08x}"
if __name__ == "__main__":
    a, b = int(sys.argv[1]), int(sys.argv[2]); bug = int(sys.argv[3]) if len(sys.argv) > 3 else 0; ticks = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    for seed in range(a, b + 1):
        c, s = run(seed, ticks); print(line(seed, c, s))
