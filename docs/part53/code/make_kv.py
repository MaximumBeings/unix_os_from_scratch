#!/usr/bin/env python3
"""Chapter 53: writes data/kv/demo.txt, the scripted workload the kernel runs against the store on the real FAT16 disk: an INVENTED warehouse's stock list (SKU -> description and quantity). Same command language as native/lsm_cli.c (put, del, get, flush, compact, digest, stats).
Phases: load 70 SKUs (the memtable fills and flushes several times; four level-0 tables trigger a compaction), reads (hits, misses, a deleted SKU), restock and sell-through updates, discontinued SKUs deleted, a second compaction. Fully deterministic."""
import os, random
R = random.Random(53); out = []
ITEMS = ["bolt", "nut", "washer", "screw", "bracket", "hinge", "latch", "spring", "gasket", "bearing", "valve", "clamp"]
def desc(i): return "%s-%s:qty=%d" % (ITEMS[i % len(ITEMS)], "abcdefghij"[i % 10] * (6 + i % 9), 20 + (i * 37) % 400)
for i in range(70): out.append(f"put sku:{1000 + i} {desc(i)}")
out += ["digest", "stats"]
for i in (3, 17, 42, 69): out.append(f"get sku:{1000 + i}")
out += ["get sku:9999", "get sku:0001", "stats"]
for i in range(0, 70, 7): out.append(f"put sku:{1000 + i} {desc(i + 1)}")
for i in (5, 6, 7, 8, 9, 10, 11, 12): out.append(f"del sku:{1000 + i}")
out += ["get sku:1005", "get sku:1007", "flush", "digest", "compact", "stats"]
for i in range(70, 95): out.append(f"put sku:{1000 + i} {desc(i)}")
out += ["get sku:1080", "get sku:1010", "get sku:1070", "digest", "stats"]
d = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "kv"); os.makedirs(d, exist_ok=True); open(os.path.join(d, "demo.txt"), "w").write("\n".join(out) + "\n"); print(len(out), "commands")
