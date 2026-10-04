/* Chapter 53: the LSM key-value store (see 055_lsm.h). Freestanding: no libc, no libgcc (only 32-bit arithmetic is used). */
#include "055_lsm.h"
#include "055_sha256.h"

static void p16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void p32(uint8_t *p, uint32_t v) { p16(p, v); p16(p + 2, v >> 16); }
static uint32_t g16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t g32(const uint8_t *p) { return g16(p) | (g16(p + 2) << 16); }
static void cpy(uint8_t *d, const uint8_t *s, uint32_t n) { for (uint32_t i = 0; i < n; i++) { d[i] = s[i]; } }
static int kcmp(const uint8_t *a, uint32_t al, const uint8_t *b, uint32_t bl) { uint32_t n = al < bl ? al : bl; for (uint32_t i = 0; i < n; i++) { if (a[i] != b[i]) { return a[i] < b[i] ? -1 : 1; } } return al == bl ? 0 : (al < bl ? -1 : 1); }

uint32_t lsm_crc32(const uint8_t *p, uint32_t n) { uint32_t c = 0xFFFFFFFFu; for (uint32_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) { c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); } } return ~c; }

/* ---- Bloom filter: 10 bits per key, 4 probes by double hashing (FNV-1a and a multiplicative hash) ---- */
static uint32_t hash1(const uint8_t *k, uint32_t n) { uint32_t h = 2166136261u; for (uint32_t i = 0; i < n; i++) { h ^= k[i]; h *= 16777619u; } return h; }
static uint32_t hash2(const uint8_t *k, uint32_t n) { uint32_t h = 5381u; for (uint32_t i = 0; i < n; i++) { h = h * 33u ^ k[i]; } return h | 1u; }
static void bloom_set(uint8_t *bits, uint32_t nbytes, const uint8_t *k, uint32_t kl) { uint32_t nb = nbytes * 8u, a = hash1(k, kl), b = hash2(k, kl); for (uint32_t i = 0; i < 4; i++) { uint32_t bit = (a + i * b) % nb; bits[bit >> 3] |= (uint8_t)(1u << (bit & 7u)); } }
static int bloom_has(const uint8_t *bits, uint32_t nbytes, const uint8_t *k, uint32_t kl) { uint32_t nb = nbytes * 8u, a = hash1(k, kl), b = hash2(k, kl); for (uint32_t i = 0; i < 4; i++) { uint32_t bit = (a + i * b) % nb; if (!(bits[bit >> 3] & (1u << (bit & 7u)))) { return 0; } } return 1; }
static uint32_t bloom_bytes(uint32_t n) { uint32_t b = (n * 10u + 7u) / 8u; return b < 8u ? 8u : b; }

static void tname(uint32_t id, char *o) { o[0] = 'T'; for (int i = 5; i >= 1; i--) { o[i] = (char)('0' + id % 10u); id /= 10u; } o[6] = '.'; o[7] = 'S'; o[8] = 'S'; o[9] = 'T'; o[10] = 0; }

/* ---- table files ----
 * data:   entries, each  klen u8 | flags u8 (bit 0 = tombstone) | vlen u16 | seq u32 | key | value   sorted by key, unique
 * index:  one entry per LSM_IDX_EVERY data entries:  offset u32 | klen u8 | key
 * bloom:  the Bloom filter bits
 * footer: 44 bytes, all u32 little endian: magic "LSM1" | n | data_len | index_off | index_len | bloom_off | bloom_len | min_seq | max_seq | 0 | CRC-32 of every byte before it */
#define FOOT 44u
#define TMAGIC 0x314D534Cu
static uint32_t ent_size(const lsm_ent_t *e) { return 8u + e->klen + e->vlen; }
static void ent_put(uint8_t *p, const lsm_ent_t *e) { p[0] = e->klen; p[1] = e->tomb; p16(p + 2, e->vlen); p32(p + 4, e->seq); cpy(p + 8, e->key, e->klen); cpy(p + 8 + e->klen, e->val, e->vlen); }
static int ent_get(const uint8_t *p, uint32_t avail, lsm_ent_t *e) { /* 0 ok, -1 malformed */
    if (avail < 8u) { return -1; } e->klen = p[0]; e->tomb = p[1]; e->vlen = (uint16_t)g16(p + 2); e->seq = g32(p + 4);
    if (e->klen < 1 || e->klen > LSM_MAX_KEY || e->tomb > 1 || e->vlen > LSM_MAX_VAL || (e->tomb && e->vlen != 0) || avail < 8u + e->klen + e->vlen) { return -1; }
    cpy(e->key, p + 8, e->klen); cpy(e->val, p + 8 + e->klen, e->vlen); return 0;
}
int lsm_table_parse(const uint8_t *buf, uint32_t size, lsm_tab_t *o) {
    if (size < FOOT + 8u || size > LSM_IO_BYTES) { return -1; }
    const uint8_t *f = buf + size - FOOT; if (g32(f) != TMAGIC || g32(f + 40) != lsm_crc32(buf, size - 4u) || g32(f + 36) != 0) { return -1; }
    uint32_t n = g32(f + 4), dl = g32(f + 8), io = g32(f + 12), il = g32(f + 16), bo = g32(f + 20), bl = g32(f + 24), mins = g32(f + 28), maxs = g32(f + 32);
    if (n < 1 || n > LSM_MAX_ENTRIES || io != dl || bo != io + il || bl != bloom_bytes(n) || bl > 512u || bo + bl + FOOT != size) { return -1; }
    uint32_t off = 0, cnt = 0, lo = 0xFFFFFFFFu, hi = 0, ip = io, in = 0; lsm_ent_t e, prev; prev.klen = 0;
    while (off < dl) {
        if (ent_get(buf + off, dl - off, &e)) { return -1; }
        if (cnt > 0 && kcmp(prev.key, prev.klen, e.key, e.klen) >= 0) { return -1; }
        if (cnt % LSM_IDX_EVERY == 0) { /* the index must hold exactly this entry */
            if (ip + 5u + e.klen > io + il || g32(buf + ip) != off || buf[ip + 4] != e.klen) { return -1; } for (uint32_t i = 0; i < e.klen; i++) { if (buf[ip + 5 + i] != e.key[i]) { return -1; } }
            if (in >= LSM_MAX_ENTRIES / LSM_IDX_EVERY + 1) { return -1; } o->idx[in].off = off; o->idx[in].klen = e.klen; cpy(o->idx[in].key, e.key, e.klen); ip += 5u + e.klen; in++;
        }
        if (cnt == 0) { o->minlen = e.klen; cpy(o->minkey, e.key, e.klen); }
        if (e.seq < lo) { lo = e.seq; } if (e.seq > hi) { hi = e.seq; }
        prev = e; off += ent_size(&e); cnt++;
    }
    if (off != dl || cnt != n || ip != io + il || lo != mins || hi != maxs) { return -1; }
    o->maxlen = prev.klen; cpy(o->maxkey, prev.key, prev.klen); o->n = n; o->size = size; o->data_len = dl; o->idx_n = in; o->min_seq = mins; o->max_seq = maxs; o->bloom_len = (uint16_t)bl; cpy(o->bloom, buf + bo, bl);
    return 0;
}

typedef struct { uint8_t *buf; uint32_t len, n; } bld_t;
static int bld_add(bld_t *b, const lsm_ent_t *e) { if (b->n >= LSM_MAX_ENTRIES) { return LSM_ERR_FULL; } ent_put(b->buf + b->len, e); b->len += ent_size(e); b->n++; return LSM_OK; }
static int bld_finish(bld_t *b, uint32_t id, uint8_t level, lsm_tab_t *meta) { /* appends index, bloom and footer, then re-parses the image into meta */
    uint32_t dl = b->len, off = 0, cnt = 0, mins = 0xFFFFFFFFu, maxs = 0; lsm_ent_t e; uint8_t *p = b->buf + dl;
    while (off < dl) { ent_get(b->buf + off, dl - off, &e); if (cnt % LSM_IDX_EVERY == 0) { p32(p, off); p[4] = e.klen; cpy(p + 5, e.key, e.klen); p += 5u + e.klen; } if (e.seq < mins) { mins = e.seq; } if (e.seq > maxs) { maxs = e.seq; } off += ent_size(&e); cnt++; }
    uint32_t il = (uint32_t)(p - (b->buf + dl)), bl = bloom_bytes(b->n); uint8_t *bits = p; for (uint32_t i = 0; i < bl; i++) { bits[i] = 0; }
    for (off = 0; off < dl;) { ent_get(b->buf + off, dl - off, &e); bloom_set(bits, bl, e.key, e.klen); off += ent_size(&e); }
    p += bl; p32(p, TMAGIC); p32(p + 4, b->n); p32(p + 8, dl); p32(p + 12, dl); p32(p + 16, il); p32(p + 20, dl + il); p32(p + 24, bl); p32(p + 28, mins); p32(p + 32, maxs); p32(p + 36, 0);
    uint32_t total = (uint32_t)(p - b->buf) + 44u; p32(p + 40, lsm_crc32(b->buf, total - 4u)); b->len = total;
    if (lsm_table_parse(b->buf, total, meta)) { return LSM_ERR_CORRUPT; } meta->id = id; meta->level = level; return LSM_OK;
}

/* ---- the manifest: two alternating slots MANI0 / MANI1, each  "MNFT" | gen | next_id | flushed_seq | ntab u16 | ntab x (id u32, level u8) | CRC-32 ---- */
static int write_manifest(lsm_t *s, const lsm_tab_t *first, uint32_t next_id, uint32_t flushed_seq, const lsm_tab_t *old, uint32_t nold, uint32_t skip_all) {
    uint8_t m[96]; uint32_t nt = (first ? 1u : 0u) + (skip_all ? 0u : nold), o = 0; p32(m, 0x54464E4Du); p32(m + 4, s->gen + 1u); p32(m + 8, next_id); p32(m + 12, flushed_seq); p16(m + 16, nt); o = 18;
    if (first) { p32(m + o, first->id); m[o + 4] = first->level; o += 5; }
    if (!skip_all) { for (uint32_t i = 0; i < nold; i++) { p32(m + o, old[i].id); m[o + 4] = old[i].level; o += 5; } }
    p32(m + o, lsm_crc32(m, o)); o += 4; char nm[6] = {'M', 'A', 'N', 'I', (char)('0' + ((s->gen + 1u) & 1u)), 0};
    if (s->fs.write(s->fs.ctx, nm, m, o)) { s->failed = 1; return LSM_ERR_IO; } return LSM_OK;
}

/* ---- the memtable ---- */
static int mem_find(const lsm_t *s, const uint8_t *k, uint32_t kl, uint32_t *pos) { uint32_t lo = 0, hi = s->nmem; while (lo < hi) { uint32_t mid = (lo + hi) / 2u; int c = kcmp(s->mem[mid].key, s->mem[mid].klen, k, kl); if (c == 0) { *pos = mid; return 1; } if (c < 0) { lo = mid + 1u; } else { hi = mid; } } *pos = lo; return 0; }
static void mem_apply(lsm_t *s, const lsm_ent_t *e) {
    uint32_t pos; if (mem_find(s, e->key, e->klen, &pos)) { s->membytes -= ent_size(&s->mem[pos]); }
    else { for (uint32_t i = s->nmem; i > pos; i--) { s->mem[i] = s->mem[i - 1]; } s->nmem++; }
    s->mem[pos] = *e; s->membytes += ent_size(e);
}

/* ---- a merge over the memtable and every table: newest source wins for equal keys ---- */
typedef struct { int is_mem; uint32_t ti, pos; int valid; lsm_ent_t e; } cur_t;
static int cur_next(lsm_t *s, cur_t *c) { /* advances to the next entry; sets c->valid; LSM_OK or an error */
    if (c->is_mem) { if (c->pos < s->nmem) { c->e = s->mem[c->pos++]; c->valid = 1; } else { c->valid = 0; } return LSM_OK; }
    const lsm_tab_t *t = &s->tab[c->ti]; if (c->pos >= t->data_len) { c->valid = 0; return LSM_OK; }
    char nm[11]; tname(t->id, nm); uint32_t got = 0, want = t->data_len - c->pos; if (want > 8u + LSM_MAX_KEY + LSM_MAX_VAL) { want = 8u + LSM_MAX_KEY + LSM_MAX_VAL; }
    if (s->fs.read(s->fs.ctx, nm, c->pos, s->blk, want, &got)) { s->failed = 1; return LSM_ERR_IO; }
    if (ent_get(s->blk, got, &c->e)) { return LSM_ERR_CORRUPT; } c->pos += ent_size(&c->e); c->valid = 1; return LSM_OK;
}
typedef int (*emit_fn)(void *ctx, const lsm_ent_t *e);
static int merge_run(lsm_t *s, int with_mem, emit_fn emit, void *ctx) {
    cur_t cur[LSM_MAX_TABLES + 1]; uint32_t nc = 0; int rc;
    if (with_mem) { cur[nc].is_mem = 1; cur[nc].ti = 0; cur[nc].pos = 0; nc++; }
    for (uint32_t i = 0; i < s->ntab; i++) { cur[nc].is_mem = 0; cur[nc].ti = i; cur[nc].pos = 0; nc++; }
    for (uint32_t i = 0; i < nc; i++) { if ((rc = cur_next(s, &cur[i]))) { return rc; } }
    for (;;) {
        int w = -1; for (uint32_t i = 0; i < nc; i++) { if (cur[i].valid && (w < 0 || kcmp(cur[i].e.key, cur[i].e.klen, cur[w].e.key, cur[w].e.klen) < 0)) { w = (int)i; } }
        if (w < 0) { return LSM_OK; }
        lsm_ent_t win = cur[w].e; if ((rc = emit(ctx, &win))) { return rc; }
        for (uint32_t i = 0; i < nc; i++) { if (cur[i].valid && kcmp(cur[i].e.key, cur[i].e.klen, win.key, win.klen) == 0) { if ((rc = cur_next(s, &cur[i]))) { return rc; } } }
    }
}

/* ---- flush and compaction ---- */
static uint32_t count_l0(const lsm_t *s) { uint32_t n = 0; for (uint32_t i = 0; i < s->ntab; i++) { if (s->tab[i].level == 0) { n++; } } return n; }
static int emit_build(void *ctx, const lsm_ent_t *e) { return e->tomb ? LSM_OK : bld_add((bld_t *)ctx, e); } /* the bottom level keeps no tombstones */
int lsm_compact(lsm_t *s) {
    if (s->failed) { return LSM_ERR_IO; } if (s->ntab == 0 || (s->ntab == 1 && s->tab[0].level == 1)) { return LSM_OK; } if (s->next_id > 99999u) { return LSM_ERR_FULL; }
    bld_t b = {s->io, 0, 0}; int rc = merge_run(s, 0, emit_build, &b); if (rc) { return rc; }
    uint32_t id = s->next_id; lsm_tab_t keep[LSM_MAX_TABLES]; uint32_t nold = s->ntab; for (uint32_t i = 0; i < nold; i++) { keep[i].id = s->tab[i].id; keep[i].level = s->tab[i].level; }
    if (b.n > 0) {
        lsm_tab_t meta; if ((rc = bld_finish(&b, id, 1, &meta))) { return rc; } char nm[11]; tname(id, nm);
        if (s->fs.write(s->fs.ctx, nm, s->io, b.len)) { s->failed = 1; return LSM_ERR_IO; }
        if ((rc = write_manifest(s, &meta, id + 1u, s->flushed_seq, keep, nold, 1))) { return rc; } s->gen++; s->next_id = id + 1u;
        for (uint32_t i = 0; i < nold; i++) { char on[11]; tname(keep[i].id, on); s->fs.del(s->fs.ctx, on); } s->tab[0] = meta; s->ntab = 1;
    } else {
        if ((rc = write_manifest(s, 0, id + 1u, s->flushed_seq, keep, nold, 1))) { return rc; } s->gen++; s->next_id = id + 1u;
        for (uint32_t i = 0; i < nold; i++) { char on[11]; tname(keep[i].id, on); s->fs.del(s->fs.ctx, on); } s->ntab = 0;
    }
    s->st.compactions++; return LSM_OK;
}
int lsm_flush(lsm_t *s) {
    if (s->failed) { return LSM_ERR_IO; } if (s->nmem == 0) { return LSM_OK; } if (s->ntab >= LSM_MAX_TABLES || s->next_id > 99999u) { return LSM_ERR_FULL; }
    bld_t b = {s->io, 0, 0}; int rc; for (uint32_t i = 0; i < s->nmem; i++) { if ((rc = bld_add(&b, &s->mem[i]))) { return rc; } }
    uint32_t id = s->next_id; lsm_tab_t meta; if ((rc = bld_finish(&b, id, 0, &meta))) { return rc; } char nm[11]; tname(id, nm);
    if (s->fs.write(s->fs.ctx, nm, s->io, b.len)) { s->failed = 1; return LSM_ERR_IO; }
    lsm_tab_t keep[LSM_MAX_TABLES]; for (uint32_t i = 0; i < s->ntab; i++) { keep[i].id = s->tab[i].id; keep[i].level = s->tab[i].level; }
    if ((rc = write_manifest(s, &meta, id + 1u, s->seq, keep, s->ntab, 0))) { return rc; }
    for (uint32_t i = s->ntab; i > 0; i--) { s->tab[i] = s->tab[i - 1]; } s->tab[0] = meta; s->ntab++; s->gen++; s->next_id = id + 1u; s->flushed_seq = s->seq; s->nmem = 0; s->membytes = 0; s->st.flushes++;
    if (s->fs.write(s->fs.ctx, "WAL", s->io, 0)) { s->failed = 1; return LSM_ERR_IO; } s->wal_bytes = 0;
    if (count_l0(s) >= LSM_L0_MAX) { return lsm_compact(s); } return LSM_OK;
}

/* ---- writes ---- */
static int mutate(lsm_t *s, const uint8_t *key, uint32_t klen, const uint8_t *val, uint32_t vlen, int tomb) {
    if (s->failed) { return LSM_ERR_IO; } if (!key || klen < 1 || klen > LSM_MAX_KEY || vlen > LSM_MAX_VAL || (vlen && !val)) { return LSM_ERR_ARG; }
    lsm_ent_t e; e.klen = (uint8_t)klen; e.tomb = (uint8_t)tomb; e.vlen = (uint16_t)vlen; e.seq = s->seq + 1u; cpy(e.key, key, klen); cpy(e.val, val, vlen);
    uint32_t pos, rec = 6u + 8u + klen + vlen; int exists = mem_find(s, key, klen, &pos); uint32_t nb = s->membytes + ent_size(&e) - (exists ? ent_size(&s->mem[pos]) : 0u);
    if (s->nmem > 0 && ((!exists && s->nmem >= LSM_MEM_MAX) || nb > LSM_MEM_BYTES || s->wal_bytes + rec > LSM_WAL_MAX)) { int rc = lsm_flush(s); if (rc) { return rc; } }
    uint8_t r[6 + 8 + LSM_MAX_KEY + LSM_MAX_VAL]; p16(r + 4, 8u + klen + vlen); p32(r + 6, e.seq); r[10] = (uint8_t)tomb; r[11] = (uint8_t)klen; p16(r + 12, vlen); cpy(r + 14, key, klen); cpy(r + 14 + klen, val, vlen);
    p32(r, lsm_crc32(r + 4, rec - 4u)); if (s->fs.append(s->fs.ctx, "WAL", r, rec)) { s->failed = 1; return LSM_ERR_IO; }
    s->seq = e.seq; s->wal_bytes += rec; mem_apply(s, &e); if (tomb) { s->st.deletes++; } else { s->st.puts++; } return LSM_OK;
}
int lsm_put(lsm_t *s, const uint8_t *key, uint32_t klen, const uint8_t *val, uint32_t vlen) { return mutate(s, key, klen, val, vlen, 0); }
int lsm_delete(lsm_t *s, const uint8_t *key, uint32_t klen) { return mutate(s, key, klen, 0, 0, 1); }

/* ---- reads ---- */
int lsm_get(lsm_t *s, const uint8_t *key, uint32_t klen, uint8_t *out, uint32_t cap, uint32_t *vlen) {
    if (!key || klen < 1 || klen > LSM_MAX_KEY) { return LSM_ERR_ARG; } s->st.gets++; uint32_t pos; lsm_ent_t e;
    if (mem_find(s, key, klen, &pos)) { e = s->mem[pos]; goto found; }
    for (uint32_t ti = 0; ti < s->ntab; ti++) {
        const lsm_tab_t *t = &s->tab[ti];
        if (kcmp(key, klen, t->minkey, t->minlen) < 0 || kcmp(key, klen, t->maxkey, t->maxlen) > 0) { s->st.range_skips++; continue; }
        if (!bloom_has(t->bloom, t->bloom_len, key, klen)) { s->st.bloom_skips++; continue; }
        uint32_t lo = 0, hi = t->idx_n; /* the last index entry whose key is <= key */
        while (lo + 1u < hi) { uint32_t mid = (lo + hi) / 2u; if (kcmp(t->idx[mid].key, t->idx[mid].klen, key, klen) <= 0) { lo = mid; } else { hi = mid; } }
        uint32_t from = t->idx[lo].off, to = lo + 1u < t->idx_n ? t->idx[lo + 1u].off : t->data_len, got = 0; char nm[11]; tname(t->id, nm); s->st.block_reads++;
        if (s->fs.read(s->fs.ctx, nm, from, s->blk, to - from, &got) || got != to - from) { return LSM_ERR_IO; }
        for (uint32_t off = 0; off < got;) { lsm_ent_t x; if (ent_get(s->blk + off, got - off, &x)) { return LSM_ERR_CORRUPT; } int c = kcmp(x.key, x.klen, key, klen); if (c == 0) { e = x; goto found; } if (c > 0) { break; } off += ent_size(&x); }
    }
    return 0;
found:
    if (e.tomb) { return 0; } if (vlen) { *vlen = e.vlen; } for (uint32_t i = 0; i < e.vlen && i < cap; i++) { out[i] = e.val[i]; } return 1;
}
typedef struct { sha256_ctx_t h; uint32_t n; } dig_t;
static int emit_digest(void *ctx, const lsm_ent_t *e) { dig_t *d = (dig_t *)ctx; if (e->tomb) { return LSM_OK; } uint8_t hd[2] = {e->klen, (uint8_t)e->vlen}; sha256_update(&d->h, hd, 2); sha256_update(&d->h, e->key, e->klen); sha256_update(&d->h, e->val, e->vlen); d->n++; return LSM_OK; }
int lsm_digest(lsm_t *s, uint8_t out[32], uint32_t *count) { dig_t d; sha256_init(&d.h); d.n = 0; int rc = merge_run(s, 1, emit_digest, &d); if (rc) { return rc; } sha256_final(&d.h, out); if (count) { *count = d.n; } return LSM_OK; }

/* ---- recovery ---- */
static int load_manifest(lsm_t *s, int slot, uint32_t *gen, uint32_t *next_id, uint32_t *fseq, uint32_t *ids, uint8_t *lv, uint32_t *nt) {
    char nm[6] = {'M', 'A', 'N', 'I', (char)('0' + slot), 0}; uint32_t sz, got; if (s->fs.size(s->fs.ctx, nm, &sz) || sz < 22u || sz > 96u) { return -1; }
    if (s->fs.read(s->fs.ctx, nm, 0, s->io, sz, &got) || got != sz) { return -1; } const uint8_t *m = s->io; uint32_t n = g16(m + 16);
    if (g32(m) != 0x54464E4Du || n > LSM_MAX_TABLES || sz != 18u + 5u * n + 4u || g32(m + sz - 4u) != lsm_crc32(m, sz - 4u)) { return -1; }
    *gen = g32(m + 4); *next_id = g32(m + 8); *fseq = g32(m + 12); *nt = n; for (uint32_t i = 0; i < n; i++) { ids[i] = g32(m + 18 + 5 * i); lv[i] = m[18 + 5 * i + 4]; } return 0;
}
int lsm_open(lsm_t *s, const lsm_fs_t *fs) {
    s->fs = *fs; s->nmem = s->membytes = s->wal_bytes = 0; s->seq = s->flushed_seq = s->gen = 0; s->next_id = 1; s->ntab = 0; s->failed = 0; s->st = (lsm_stats_t){0};
    uint32_t bg = 0, bn = 1, bf = 0, bids[LSM_MAX_TABLES], bt = 0; uint8_t blv[LSM_MAX_TABLES]; int have = 0;
    for (int slot = 0; slot < 2; slot++) {
        uint32_t g, n, f, ids[LSM_MAX_TABLES], nt; uint8_t lv[LSM_MAX_TABLES]; if (load_manifest(s, slot, &g, &n, &f, ids, lv, &nt)) { continue; }
        if (!have || g > bg) { have = 1; bg = g; bn = n; bf = f; bt = nt; for (uint32_t i = 0; i < nt; i++) { bids[i] = ids[i]; blv[i] = lv[i]; } }
    }
    s->gen = bg; s->next_id = bn; s->flushed_seq = bf; s->seq = bf;
    for (uint32_t i = 0; i < bt; i++) { /* every listed table must be present and valid; levels: level-0 tables first, at most one level-1 table, last */
        if (blv[i] > 1 || (blv[i] == 1 && i + 1u != bt) || bids[i] == 0 || bids[i] >= bn) { return LSM_ERR_CORRUPT; }
        char nm[11]; tname(bids[i], nm); uint32_t sz, got; if (s->fs.size(s->fs.ctx, nm, &sz) || sz > LSM_IO_BYTES || s->fs.read(s->fs.ctx, nm, 0, s->io, sz, &got) || got != sz) { return LSM_ERR_CORRUPT; }
        if (lsm_table_parse(s->io, sz, &s->tab[i])) { return LSM_ERR_CORRUPT; } s->tab[i].id = bids[i]; s->tab[i].level = blv[i];
    }
    s->ntab = bt;
    for (uint32_t id = 1; id <= s->next_id && id <= 99999u; id++) { int live = 0; for (uint32_t i = 0; i < bt; i++) { if (bids[i] == id) { live = 1; } } if (live) { continue; } char nm[11]; tname(id, nm); uint32_t sz; if (!s->fs.size(s->fs.ctx, nm, &sz)) { s->fs.del(s->fs.ctx, nm); s->st.orphans_deleted++; } }
    uint32_t wsz = 0, got = 0, pos = 0, prev = 0; int torn = 0;
    if (!s->fs.size(s->fs.ctx, "WAL", &wsz)) {
        if (wsz > LSM_IO_BYTES || s->fs.read(s->fs.ctx, "WAL", 0, s->io, wsz, &got) || got != wsz) { return LSM_ERR_IO; }
        while (pos < wsz) {
            const uint8_t *r = s->io + pos; if (wsz - pos < 6u) { break; } uint32_t pl = g16(r + 4); if (pl < 8u || pl > 8u + LSM_MAX_KEY + LSM_MAX_VAL || wsz - pos < 6u + pl) { break; }
            if (g32(r) != lsm_crc32(r + 4, 2u + pl)) { break; }
            lsm_ent_t e; e.seq = g32(r + 6); e.tomb = r[10]; e.klen = r[11]; e.vlen = (uint16_t)g16(r + 12);
            if (e.klen < 1 || e.klen > LSM_MAX_KEY || e.tomb > 1 || e.vlen > LSM_MAX_VAL || (e.tomb && e.vlen) || pl != 8u + e.klen + e.vlen || e.seq <= prev) { break; }
            cpy(e.key, r + 14, e.klen); cpy(e.val, r + 14 + e.klen, e.vlen); prev = e.seq;
            if (e.seq > s->flushed_seq) { mem_apply(s, &e); s->st.wal_replayed++; if (e.seq > s->seq) { s->seq = e.seq; } }
            pos += 6u + pl;
        }
        torn = pos != wsz; s->st.wal_cut_bytes = wsz - pos; s->wal_bytes = pos;
    }
    if (s->nmem > LSM_MEM_MAX) { return LSM_ERR_CORRUPT; }
    if (torn) { /* the log cannot be appended to after garbage, and cutting it in place could lose records if the cut itself were torn: flush what was replayed (crash-safe), which empties the log */
        int rc; if (s->nmem > 0) { rc = lsm_flush(s); } else { rc = s->fs.write(s->fs.ctx, "WAL", s->io, 0) ? LSM_ERR_IO : LSM_OK; if (!rc) { s->wal_bytes = 0; } else { s->failed = 1; } } if (rc) { return rc; }
    }
    return LSM_OK;
}
