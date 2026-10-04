/* Chapter 53 host tools: a RAM file system with a CRASH INJECTOR. Every operation has a cost: write and append cost len+1, delete costs 1, reads are free. When the remaining budget is smaller than an operation's cost, the operation is TORN
 * (write: the file becomes the first `budget` bytes of the new content; append: the first `budget` bytes are appended; delete: not done), the file system is marked crashed and every later call fails. budget < 0 means unlimited. */
#ifndef RAMFS_H
#define RAMFS_H
#include <stdint.h>
#include <string.h>
#include "../053_lsm.h"
#define RF_FILES 16
#define RF_CAP 28000
typedef struct { char name[16]; int used; uint32_t len; uint8_t d[RF_CAP]; } rf_file_t;
typedef struct { rf_file_t f[RF_FILES]; long budget; long spent; int crashed; } ramfs_t;
static rf_file_t *rf_find(ramfs_t *r, const char *n) { for (int i = 0; i < RF_FILES; i++) { if (r->f[i].used && !strcmp(r->f[i].name, n)) { return &r->f[i]; } } return 0; }
static rf_file_t *rf_make(ramfs_t *r, const char *n) { rf_file_t *f = rf_find(r, n); if (f) { return f; } for (int i = 0; i < RF_FILES; i++) { if (!r->f[i].used) { r->f[i].used = 1; strcpy(r->f[i].name, n); r->f[i].len = 0; return &r->f[i]; } } return 0; }
static int rf_charge(ramfs_t *r, long cost, long *allowed) { /* 1 = completes; 0 = torn after *allowed units */ if (r->crashed) { *allowed = 0; return -1; } if (r->budget < 0 || r->budget >= cost) { if (r->budget >= 0) { r->budget -= cost; } r->spent += cost; *allowed = cost; return 1; } *allowed = r->budget; r->spent += r->budget; r->budget = 0; r->crashed = 1; return 0; }
static int rf_read(void *c, const char *n, uint32_t off, uint8_t *buf, uint32_t len, uint32_t *got) { ramfs_t *r = c; rf_file_t *f = rf_find(r, n); if (!f) { return 1; } uint32_t k = off >= f->len ? 0 : (f->len - off < len ? f->len - off : len); memcpy(buf, f->d + off, k); *got = k; return 0; }
static int rf_size(void *c, const char *n, uint32_t *sz) { rf_file_t *f = rf_find(c, n); if (!f) { return 1; } *sz = f->len; return 0; }
static int rf_write(void *c, const char *n, const uint8_t *d, uint32_t len) { ramfs_t *r = c; long al; int k = rf_charge(r, (long)len + 1, &al); if (k < 0) { return 1; } rf_file_t *f = rf_make(r, n); if (!f || len > RF_CAP) { return 2; } uint32_t w = k ? len : (uint32_t)al; memcpy(f->d, d, w); f->len = w; return k ? 0 : 1; }
static int rf_append(void *c, const char *n, const uint8_t *d, uint32_t len) { ramfs_t *r = c; long al; int k = rf_charge(r, (long)len + 1, &al); if (k < 0) { return 1; } rf_file_t *f = rf_make(r, n); if (!f || f->len + len > RF_CAP) { return 2; } uint32_t w = k ? len : (uint32_t)al; memcpy(f->d + f->len, d, w); f->len += w; return k ? 0 : 1; }
static int rf_del(void *c, const char *n) { ramfs_t *r = c; long al; int k = rf_charge(r, 1, &al); if (k <= 0) { return 1; } rf_file_t *f = rf_find(r, n); if (f) { f->used = 0; } return 0; }
static lsm_fs_t rf_fs(ramfs_t *r) { lsm_fs_t f = {r, rf_read, rf_write, rf_append, rf_del, rf_size}; return f; }
static void rf_clone(ramfs_t *dst, const ramfs_t *src) { for (int i = 0; i < RF_FILES; i++) { dst->f[i].used = src->f[i].used; if (src->f[i].used) { strcpy(dst->f[i].name, src->f[i].name); dst->f[i].len = src->f[i].len; memcpy(dst->f[i].d, src->f[i].d, src->f[i].len); } } dst->budget = -1; dst->spent = 0; dst->crashed = 0; }
static void rf_init(ramfs_t *r) { for (int i = 0; i < RF_FILES; i++) { r->f[i].used = 0; } r->budget = -1; r->spent = 0; r->crashed = 0; }
#endif
