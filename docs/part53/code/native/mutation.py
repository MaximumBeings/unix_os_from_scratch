#!/usr/bin/env python3
"""READ THIS FIRST: this script deliberately BREAKS the chapter's sources, one line at a time, in a temporary copy, and runs the host-side test suite against each broken copy. It expects the suite to FAIL every time. A "caught" line is the EXPECTED, wanted
result: it shows the tests can detect that mistake. "NOT CAUGHT" would be a gap. The real sources are never modified. For each mutant it runs: lsm_test (worked by hand), the differential test against the independent Python store (30 random scripts with crashes), the crash test (a crash at every 3rd byte of a 60-operation workload, and during recovery) and the fuzzer. Four broken copies are tested at a time. Output: mutation_out.txt   Usage: mutation.py"""
import concurrent.futures, os, shutil, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); code = os.path.join(here, "..")
MUT = [
 ('053_lsm.c', 'CRC: wrong polynomial', '0xEDB88320u & (0u - (c & 1u))', '0xEDB88321u & (0u - (c & 1u))'),
 ('053_lsm.c', 'CRC: final inversion missing', 'return ~c; }\n\n/* ---- Bloom', 'return c; }\n\n/* ---- Bloom'),
 ('053_lsm.c', 'Bloom: only 3 probes', 'for (uint32_t i = 0; i < 4; i++) { uint32_t bit = (a + i * b) % nb; bits[bit >> 3] |=', 'for (uint32_t i = 0; i < 3; i++) { uint32_t bit = (a + i * b) % nb; bits[bit >> 3] |='),
 ('053_lsm.c', 'Bloom: the lookup uses 3 probes while the filter was built with 4 (never a false negative, but different)', 'for (uint32_t i = 0; i < 4; i++) { uint32_t bit = (a + i * b) % nb; if (!(bits', 'for (uint32_t i = 0; i < 3; i++) { uint32_t bit = (a + i * b) % nb; if (!(bits'),
 ('053_lsm.c', 'Bloom: 8 bits per key', 'uint32_t b = (n * 10u + 7u) / 8u;', 'uint32_t b = (n * 8u + 7u) / 8u;'),
 ('053_lsm.c', 'Bloom: the filter is never consulted', 'if (!bloom_has(t->bloom, t->bloom_len, key, klen)) { s->st.bloom_skips++; continue; }', ''),
 ('053_lsm.c', 'range check: the minimum key is exclusive', 'if (kcmp(key, klen, t->minkey, t->minlen) < 0 || kcmp(key, klen, t->maxkey, t->maxlen) > 0)', 'if (kcmp(key, klen, t->minkey, t->minlen) <= 0 || kcmp(key, klen, t->maxkey, t->maxlen) > 0)'),
 ('053_lsm.c', 'range check: the maximum key is exclusive', 'kcmp(key, klen, t->maxkey, t->maxlen) > 0) { s->st.range_skips++;', 'kcmp(key, klen, t->maxkey, t->maxlen) >= 0) { s->st.range_skips++;'),
 ('053_lsm.c', 'key order: a shorter key sorts after a longer one with the same prefix', 'return al == bl ? 0 : (al < bl ? -1 : 1); }\n\nuint32_t lsm_crc32', 'return al == bl ? 0 : (al < bl ? 1 : -1); }\n\nuint32_t lsm_crc32'),
 ('053_lsm.c', 'key order: bytes compared as signed', 'if (a[i] != b[i]) { return a[i] < b[i] ? -1 : 1; } } return al', 'if (a[i] != b[i]) { return (int8_t)a[i] < (int8_t)b[i] ? -1 : 1; } } return al'),
 ('053_lsm.c', 'table: sparse index every 4 entries in the reader but 8 in the writer', 'if (cnt % LSM_IDX_EVERY == 0) { /* the index must hold exactly this entry */', 'if (cnt % 4 == 0) { /* the index must hold exactly this entry */'),
 ('053_lsm.c', 'table: the index is not checked against the entries', 'if (ip + 5u + e.klen > io + il || g32(buf + ip) != off || buf[ip + 4] != e.klen) { return -1; } for (uint32_t i = 0; i < e.klen; i++) { if (buf[ip + 5 + i] != e.key[i]) { return -1; } }', 'if (ip + 5u + e.klen > io + il) { return -1; }'),
 ('053_lsm.c', 'table: keys need not be strictly ascending', 'if (cnt > 0 && kcmp(prev.key, prev.klen, e.key, e.klen) >= 0) { return -1; }', 'if (cnt > 0 && kcmp(prev.key, prev.klen, e.key, e.klen) > 0) { return -1; }'),
 ('053_lsm.c', 'table: the footer CRC is not checked', '|| g32(f + 40) != lsm_crc32(buf, size - 4u) ||', '||'),
 ('053_lsm.c', 'table: the CRC covers everything except the last byte before it', 'p32(p + 40, lsm_crc32(b->buf, total - 4u));', 'p32(p + 40, lsm_crc32(b->buf, total - 5u));'),
 ('053_lsm.c', 'table: an empty table is accepted', 'if (n < 1 || n > LSM_MAX_ENTRIES ||', 'if (n > LSM_MAX_ENTRIES ||'),
 ('053_lsm.c', 'table: 401 entries accepted', 'if (n < 1 || n > LSM_MAX_ENTRIES ||', 'if (n < 1 || n > LSM_MAX_ENTRIES + 1 ||'),
 ('053_lsm.c', 'table: the builder allows 401 entries', 'if (b->n >= LSM_MAX_ENTRIES) { return LSM_ERR_FULL; }', 'if (b->n > LSM_MAX_ENTRIES) { return LSM_ERR_FULL; }'),
 ('053_lsm.c', 'table: sequence range not checked', '|| lo != mins || hi != maxs) { return -1; }', ') { return -1; }'),
 ('053_lsm.c', 'entry: a 25-byte key is accepted', 'if (e->klen < 1 || e->klen > LSM_MAX_KEY ||', 'if (e->klen < 1 || e->klen > LSM_MAX_KEY + 1 ||'),
 ('053_lsm.c', 'entry: a tombstone with a value is accepted', '|| (e->tomb && e->vlen != 0) || avail <', '|| avail <'),
 ('053_lsm.c', 'entry: the value length is 8 bits', 'e->vlen = (uint16_t)g16(p + 2);', 'e->vlen = (uint16_t)(p[2]);'),
 ('053_lsm.c', 'manifest: the CRC is not checked', '|| g32(m + sz - 4u) != lsm_crc32(m, sz - 4u)) { return -1; }', ') { return -1; }'),
 ('053_lsm.c', 'manifest: the OLDER generation wins', 'if (!have || g > bg) {', 'if (!have || g < bg) {'),
 ('053_lsm.c', 'manifest: slot chosen by the old generation (overwrites the slot in use)', "char nm[6] = {'M', 'A', 'N', 'I', (char)('0' + ((s->gen + 1u) & 1u)), 0};", "char nm[6] = {'M', 'A', 'N', 'I', (char)('0' + (s->gen & 1u)), 0};"),
 ('053_lsm.c', 'manifest: flushed_seq not recorded on a flush', 'write_manifest(s, &meta, id + 1u, s->seq, keep, s->ntab, 0)', 'write_manifest(s, &meta, id + 1u, s->flushed_seq, keep, s->ntab, 0)'),
 ('053_lsm.c', 'manifest: next_id not advanced on a flush', 'write_manifest(s, &meta, id + 1u, s->seq, keep, s->ntab, 0)', 'write_manifest(s, &meta, id, s->seq, keep, s->ntab, 0)'),
 ('053_lsm.c', 'manifest: a flush forgets the old tables', 'write_manifest(s, &meta, id + 1u, s->seq, keep, s->ntab, 0)', 'write_manifest(s, &meta, id + 1u, s->seq, keep, s->ntab, 1)'),
 ('053_lsm.c', 'manifest: a level-1 table in the middle of the list is accepted', '(blv[i] == 1 && i + 1u != bt) ||', '||'),
 ('053_lsm.c', 'manifest: a table id of 0 is accepted', '|| bids[i] == 0 || bids[i] >= bn)', '|| bids[i] >= bn)'),
 ('053_lsm.c', 'manifest: a table id at or above next_id is accepted', '|| bids[i] == 0 || bids[i] >= bn)', '|| bids[i] == 0)'),
 ('053_lsm.c', 'flush: the table is written AFTER the manifest names it', 'if (s->fs.write(s->fs.ctx, nm, s->io, b.len)) { s->failed = 1; return LSM_ERR_IO; }\n    lsm_tab_t keep[LSM_MAX_TABLES]; for (uint32_t i = 0; i < s->ntab; i++) { keep[i].id = s->tab[i].id; keep[i].level = s->tab[i].level; }\n    if ((rc = write_manifest(s, &meta, id + 1u, s->seq, keep, s->ntab, 0))) { return rc; }', 'lsm_tab_t keep[LSM_MAX_TABLES]; for (uint32_t i = 0; i < s->ntab; i++) { keep[i].id = s->tab[i].id; keep[i].level = s->tab[i].level; }\n    if ((rc = write_manifest(s, &meta, id + 1u, s->seq, keep, s->ntab, 0))) { return rc; }\n    if (s->fs.write(s->fs.ctx, nm, s->io, b.len)) { s->failed = 1; return LSM_ERR_IO; }'),
 ('053_lsm.c', 'flush: the log is emptied BEFORE the table and manifest are written', 'uint32_t id = s->next_id; lsm_tab_t meta; if ((rc = bld_finish(&b, id, 0, &meta))) { return rc; } char nm[11]; tname(id, nm);', 'uint32_t id = s->next_id; lsm_tab_t meta; if ((rc = bld_finish(&b, id, 0, &meta))) { return rc; } char nm[11]; tname(id, nm); if (s->fs.write(s->fs.ctx, "WAL", s->io + 60000, 0)) { return LSM_ERR_IO; }'),
 ('053_lsm.c', 'flush: the log is not emptied', 'if (s->fs.write(s->fs.ctx, "WAL", s->io, 0)) { s->failed = 1; return LSM_ERR_IO; } s->wal_bytes = 0;', 's->wal_bytes = 0;'),
 ('053_lsm.c', 'flush: the new table goes to the END of the list (oldest position)', 'for (uint32_t i = s->ntab; i > 0; i--) { s->tab[i] = s->tab[i - 1]; } s->tab[0] = meta; s->ntab++;', 's->tab[s->ntab] = meta; s->ntab++;'),
 ('053_lsm.c', 'flush: the sequence floor is not advanced in memory', 's->flushed_seq = s->seq; s->nmem = 0;', 's->nmem = 0;'),
 ('053_lsm.c', 'flush: 12 tables are not refused', 'if (s->ntab >= LSM_MAX_TABLES || s->next_id > 99999u) { return LSM_ERR_FULL; }\n    bld_t b = {s->io, 0, 0}; int rc; for', 'if (s->next_id > 99999u) { return LSM_ERR_FULL; }\n    bld_t b = {s->io, 0, 0}; int rc; for'),
 ('053_lsm.c', 'compaction trigger: 5 level-0 tables instead of 4', 'if (count_l0(s) >= LSM_L0_MAX) { return lsm_compact(s); }', 'if (count_l0(s) > LSM_L0_MAX) { return lsm_compact(s); }'),
 ('053_lsm.c', 'compaction: the older value wins', 'if (cur[i].valid && (w < 0 || kcmp(cur[i].e.key, cur[i].e.klen, cur[w].e.key, cur[w].e.klen) < 0)) { w = (int)i; }', 'if (cur[i].valid && (w < 0 || kcmp(cur[i].e.key, cur[i].e.klen, cur[w].e.key, cur[w].e.klen) <= 0)) { w = (int)i; }'),
 ('053_lsm.c', 'compaction: tombstones are kept', 'return e->tomb ? LSM_OK : bld_add((bld_t *)ctx, e); }', 'return bld_add((bld_t *)ctx, e); }'),
 ('053_lsm.c', 'compaction: the old tables are not deleted', 'for (uint32_t i = 0; i < nold; i++) { char on[11]; tname(keep[i].id, on); s->fs.del(s->fs.ctx, on); } s->tab[0] = meta; s->ntab = 1;', 's->tab[0] = meta; s->ntab = 1;'),
 ('053_lsm.c', 'compaction: the old tables are deleted BEFORE the manifest is written', 'if ((rc = write_manifest(s, &meta, id + 1u, s->flushed_seq, keep, nold, 1))) { return rc; } s->gen++; s->next_id = id + 1u;\n        for (uint32_t i = 0; i < nold; i++) { char on[11]; tname(keep[i].id, on); s->fs.del(s->fs.ctx, on); } s->tab[0] = meta;', 'for (uint32_t i = 0; i < nold; i++) { char on[11]; tname(keep[i].id, on); s->fs.del(s->fs.ctx, on); } if ((rc = write_manifest(s, &meta, id + 1u, s->flushed_seq, keep, nold, 1))) { return rc; } s->gen++; s->next_id = id + 1u; s->tab[0] = meta;'),
 ('053_lsm.c', 'compaction: flushed_seq reset to 0 in the manifest', 'write_manifest(s, &meta, id + 1u, s->flushed_seq, keep, nold, 1)', 'write_manifest(s, &meta, id + 1u, 0, keep, nold, 1)'),
 ('053_lsm.c', 'compaction: a single level-1 table is compacted again', 'if (s->ntab == 0 || (s->ntab == 1 && s->tab[0].level == 1)) { return LSM_OK; }', 'if (s->ntab == 0) { return LSM_OK; }'),
 ('053_lsm.c', 'compaction: an empty result is not handled (the old tables stay listed)', '        if ((rc = write_manifest(s, 0, id + 1u, s->flushed_seq, keep, nold, 1))) { return rc; } s->gen++; s->next_id = id + 1u;\n        for (uint32_t i = 0; i < nold; i++) { char on[11]; tname(keep[i].id, on); s->fs.del(s->fs.ctx, on); } s->ntab = 0;', '        s->ntab = 0;'),
 ('053_lsm.c', 'compaction: the level of the output is 0', 'if ((rc = bld_finish(&b, id, 1, &meta))) { return rc; } char nm[11]; tname(id, nm);\n        if (s->fs.write', 'if ((rc = bld_finish(&b, id, 0, &meta))) { return rc; } char nm[11]; tname(id, nm);\n        if (s->fs.write'),
 ('053_lsm.c', 'write path: memtable limit 1025 bytes', 'nb > LSM_MEM_BYTES ||', 'nb > LSM_MEM_BYTES + 1 ||'),
 ('053_lsm.c', 'write path: memtable entry limit not enforced', '(!exists && s->nmem >= LSM_MEM_MAX) ||', '(0 && !exists && s->nmem >= LSM_MEM_MAX) ||'),
 ('053_lsm.c', 'write path: log limit not enforced', '|| s->wal_bytes + rec > LSM_WAL_MAX)) { int rc', '|| 0)) { int rc'),
 ('053_lsm.c', 'write path: the log limit is off by one', 's->wal_bytes + rec > LSM_WAL_MAX)) { int rc', 's->wal_bytes + rec >= LSM_WAL_MAX)) { int rc'),
 ('053_lsm.c', 'write path: overwriting a key is counted as a new entry in the byte budget', 'uint32_t nb = s->membytes + ent_size(&e) - (exists ? ent_size(&s->mem[pos]) : 0u);', 'uint32_t nb = s->membytes + ent_size(&e);'),
 ('053_lsm.c', "write path: the record's CRC omits the length field", 'p32(r, lsm_crc32(r + 4, rec - 4u));', 'p32(r, lsm_crc32(r + 6, rec - 6u));'),
 ('053_lsm.c', 'write path: the sequence number is not logged in the record (always 1)', 'p32(r + 6, e.seq);', 'p32(r + 6, 1);'),
 ('053_lsm.c', 'write path: a 101-byte value is accepted', 'vlen > LSM_MAX_VAL || (vlen && !val)', 'vlen > LSM_MAX_VAL + 1 || (vlen && !val)'),
 ('053_lsm.c', 'write path: an empty key is accepted', 'if (s->failed) { return LSM_ERR_IO; } if (!key || klen < 1 ||', 'if (s->failed) { return LSM_ERR_IO; } if (!key ||'),
 ('053_lsm.c', 'write path: a delete logs the tombstone flag as 0', 'r[10] = (uint8_t)tomb;', 'r[10] = 0;'),
 ('053_lsm.c', 'write path: after an I/O error the store keeps accepting writes', 's->failed = 1; return LSM_ERR_IO; }\n    s->seq = e.seq;', 'return LSM_ERR_IO; }\n    s->seq = e.seq;'),
 ('053_lsm.c', 'read path: a tombstone in a table is skipped, so the older value reappears', 'if (c == 0) { e = x; goto found; } if (c > 0) { break; }', 'if (c == 0) { if (x.tomb) { break; } e = x; goto found; } if (c > 0) { break; }'),
 ('053_lsm.c', 'read path: only the first index block is searched', 'while (lo + 1u < hi) { uint32_t mid = (lo + hi) / 2u; if (kcmp(t->idx[mid].key, t->idx[mid].klen, key, klen) <= 0) { lo = mid; } else { hi = mid; } }', 'hi = lo + 1u;'),
 ('053_lsm.c', 'read path: the block end is the data end (reads too much, still right) -> index of the NEXT block used as start', 'uint32_t from = t->idx[lo].off,', 'uint32_t from = t->idx[lo + 1u < t->idx_n ? lo + 1u : lo].off,'),
 ('053_lsm.c', 'read path: the value is truncated by one byte', 'for (uint32_t i = 0; i < e.vlen && i < cap; i++) { out[i] = e.val[i]; } return 1;', 'for (uint32_t i = 0; i + 1u < e.vlen && i < cap; i++) { out[i] = e.val[i]; } return 1;'),
 ('053_lsm.c', 'digest: tombstones are hashed as empty values', 'if (e->tomb) { return LSM_OK; } uint8_t hd[2]', 'uint8_t hd[2]'),
 ('053_lsm.c', 'digest: the memtable is left out', 'int rc = merge_run(s, 1, emit_digest, &d);', 'int rc = merge_run(s, 0, emit_digest, &d);'),
 ('053_lsm.c', 'recovery: records at or below the flushed sequence are replayed again', 'if (e.seq > s->flushed_seq) { mem_apply', 'if (e.seq >= s->flushed_seq) { mem_apply'),
 ('053_lsm.c', 'recovery: the sequence number is not restored from the log', 'if (e.seq > s->seq) { s->seq = e.seq; } }', '}'),
 ('053_lsm.c', "recovery: the first record's CRC is not checked", 'if (g32(r) != lsm_crc32(r + 4, 2u + pl)) { break; }', 'if (pos != 0 && g32(r) != lsm_crc32(r + 4, 2u + pl)) { break; }'),
 ('053_lsm.c', 'recovery: a record with a bad CRC is skipped instead of ending the replay', 'if (g32(r) != lsm_crc32(r + 4, 2u + pl)) { break; }', 'if (g32(r) != lsm_crc32(r + 4, 2u + pl)) { pos += 6u + pl; continue; }'),
 ('053_lsm.c', 'recovery: a non-increasing sequence number is accepted', '|| pl != 8u + e.klen + e.vlen || e.seq <= prev) { break; }', '|| pl != 8u + e.klen + e.vlen) { break; }'),
 ('053_lsm.c', 'recovery: a torn tail is not treated as torn (the log is left as is)', 'torn = pos != wsz; s->st.wal_cut_bytes', 'torn = 0; s->st.wal_cut_bytes'),
 ('053_lsm.c', 'recovery: the torn log is cut IN PLACE (rewritten with its valid prefix) instead of flushed', 'if (s->nmem > 0) { rc = lsm_flush(s); } else {', 'if (s->nmem > 0) { rc = s->fs.write(s->fs.ctx, "WAL", s->io, pos) ? LSM_ERR_IO : LSM_OK; } else {'),
 ('053_lsm.c', 'recovery: orphan tables are not deleted', 'if (!s->fs.size(s->fs.ctx, nm, &sz)) { s->fs.del(s->fs.ctx, nm); s->st.orphans_deleted++; } }', '}'),
 ('053_lsm.c', 'recovery: the table that is still listed is deleted as an orphan', 'if (live) { continue; } char nm[11]', 'char nm[11]'),
 ('053_lsm.c', 'recovery: the next table id is not checked for orphans', 'for (uint32_t id = 1; id <= s->next_id && id <= 99999u; id++)', 'for (uint32_t id = 1; id < s->next_id && id <= 99999u; id++)'),
 ('053_lsm.c', 'recovery: a corrupt listed table is ignored instead of refused', 'if (lsm_table_parse(s->io, sz, &s->tab[i])) { return LSM_ERR_CORRUPT; }', 'if (lsm_table_parse(s->io, sz, &s->tab[i])) { continue; }'),
 ('053_lsm.c', 'recovery: a missing listed table is ignored', 'if (s->fs.size(s->fs.ctx, nm, &sz) || sz > LSM_IO_BYTES || s->fs.read(s->fs.ctx, nm, 0, s->io, sz, &got) || got != sz) { return LSM_ERR_CORRUPT; }', 'if (s->fs.size(s->fs.ctx, nm, &sz) || sz > LSM_IO_BYTES || s->fs.read(s->fs.ctx, nm, 0, s->io, sz, &got) || got != sz) { continue; }'),
 ('lsm_ref.py', 'reference: ties in the merge go to the oldest table (the oracle itself broken)', 'for tid, _ in reversed(s.tabs):\n            for e in s.meta[tid]: view[e[0]] = e\n        out =', 'for tid, _ in s.tabs:\n            for e in s.meta[tid]: view[e[0]] = e\n        out ='),
]
def run(tmp, cmd, **kw): return subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, errors="replace", **kw)
SRC = ["../053_lsm.c", "../053_sha256.c"]; G = ["gcc", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]
def suite(tmp):
    failed = []; n = os.path.join(tmp, "native")
    b = run(n, G + ["lsm_test.c"] + SRC + ["-o", "lt"])
    if b.returncode != 0: failed.append("lsm_test (did not build)")
    elif run(n, ["./lt"]).returncode != 0: failed.append("lsm_test")
    b = run(n, G + ["lsm_cli.c"] + SRC + ["-o", "cli"])
    if b.returncode != 0: failed.append("the store (did not build)"); return failed
    if run(n, [sys.executable, "diff_lsm.py", "30", "./cli"], env=dict(os.environ, STOP_AT_FIRST="1")).returncode != 0: failed.append("random scripts vs the Python store")
    b = run(n, G + ["lsm_crash.c"] + SRC + ["-o", "cr"])
    if b.returncode != 0: failed.append("crash test (did not build)")
    elif run(n, ["./cr", "7", "60", "3", "4"]).returncode != 0: failed.append("crash test")
    b = run(n, G + ["lsm_fuzz.c"] + SRC + ["-o", "fz"])
    if b.returncode != 0: failed.append("fuzz (did not build)")
    elif run(n, ["./fz", "3"]).returncode != 0: failed.append("fuzz")
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
