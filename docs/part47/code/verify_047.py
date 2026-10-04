#!/usr/bin/env python3
"""Chapter 47: verification FROM OUTSIDE THE KERNEL. It reads only two files the kernel left behind -- the serial capture and the FAT16 disk image -- and shares no code with the kernel:
  1. parses the FAT16 volume on the disk image with its own reader (mkt_model.py) and extracts the write-ahead log (W0000001.LOG ...);
  2. verifies every record: magic, length, sequence number, and CRC-32 with Python's own zlib.crc32;
  3. decodes every command and REPLAYS the whole log through an independent Python marketplace (auction by literal step-by-step simulation, ledger, orders, idempotency table), checking the
     marketplace invariants after every command;
  4. recomputes the SHA-256 state hash and compares it with the hash the kernel printed;
  5. compares the kernel's own narrated results (price, leader, minimum next bid, reserve flag after every bid; balances after release) with the independent replay;
  6. parses EVERY JSON body the kernel printed with Python's json module, and checks every field name against the field names of eBay's real OpenAPI schemas (--oas DIR, a clone of
     github.com/hendt/ebay-api at commit e20388b).
Usage: verify_047.py SERIAL_TXT DISK_IMG [--oas DIR]"""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkt_model import read_volume, decode_log, Market
serial_path, disk_path = sys.argv[1], sys.argv[2]
oas_dir = sys.argv[sys.argv.index("--oas") + 1] if "--oas" in sys.argv else None
fails = 0
def report(ok, msg):
    global fails; fails += not ok; print(("  ok   " if ok else "  FAIL ") + msg)

geo, files = read_volume(disk_path)
cmds, why, nfiles = decode_log(files)
print(f"-- 1. FAT16 volume: {geo['bps']} bytes/sector, {geo['spc']} sector/cluster, {geo['nfats']} FATs x {geo['fat_size']} sectors, root at LBA {geo['root_lba']}, data at LBA {geo['data_lba']}; {nfiles} log files found")
report(nfiles > 0 and why == "log ends" and len(cmds) == nfiles, f"log files are consecutive from W0000001.LOG to W{nfiles:07d}.LOG")
print("-- 2. every log record: magic, length, sequence number, CRC-32 (Python's zlib)")
report(len(cmds) == nfiles, f"all {nfiles} records pass every check (decoding stopped because: {why})")
print("-- 3. independent replay of the whole log (invariants asserted after every command)")
mk = Market(); bid_states = []
try:
    for c in cmds:
        code, v1, v2, rep = mk.apply(c)
        if c["type"] == 2:
            l = mk.L(c["a"][0]); bid_states.append((l["price"], l["high"], l["high_max"], mk.min_bid(l), mk.reserve_met(l), l["bid_count"]))
    report(True, f"{len(cmds)} commands replayed; ledger sums to zero, escrow equals the PAID orders, no negative account, price/leader rules held after every command")
except AssertionError as e:
    report(False, "invariant violated in the independent replay: " + str(e))

# ---- 4/5. against the kernel's printed output ----
ser = open(serial_path, errors="replace").read()
print("-- 4. the state hash")
m = re.search(r"final state hash ([0-9a-f]{16})", ser); kh = m.group(1) if m else None
mine = mk.hash().hex()[:16]
report(kh == mine, f"SHA-256 state hash of the independently replayed marketplace, first 8 bytes {mine}, equals the hash the kernel printed ({kh})")
print("-- 5. the kernel's narrated results against the independent replay")
cons = re.findall(r"\[server console\] price now \$(\d+)\.(\d\d), leader user (\d+) \(standing maximum \$(\d+)\.(\d\d)\), minimum next bid \$(\d+)\.(\d\d), reserve met: (yes|no), accepted bids (\d+)", ser)
got = [(int(a) * 100 + int(b), int(c), int(d) * 100 + int(e), int(f) * 100 + int(g), h == "yes", int(i)) for (a, b, c, d, e, f, g, h, i) in cons]
report(len(got) == len(bid_states) and got == bid_states, f"after each of the {len(bid_states)} bid requests, the kernel's printed price, leader, maximum, minimum next bid, reserve flag and bid count equal the replay's")
m = re.search(r"After release:\s+ledger total (-?\d+).*?escrow \$(\d+\.\d\d); fees \$(\d+\.\d\d); users 4/5/6/3: \$(\d+\.\d\d) \$(\d+\.\d\d) \$(\d+\.\d\d) \$(\d+\.\d\d)", ser)
want = (0, "0.00", "5.30", "500.00", "500.00", "450.00", "44.70")
report(m is not None and (int(m.group(1)),) + tuple(m.groups()[1:]) == want and [f"{b // 100}.{b % 100:02d}" for b in (mk.bal[1], mk.bal[2], mk.bal[4], mk.bal[5], mk.bal[6], mk.bal[3])] == list(want[1:]),
       "balances after release (escrow, fees, users 4/5/6/3) printed by the kernel equal the replay's, and equal the arithmetic done by hand: fee = 10% of $50.00 + $0.30 = $5.30, seller receives $44.70")
hist = [(c["type"], c["a"][:3]) for c in cmds]
report(True, f"final marketplace after replay: {mk.applied} commands, listing sold: {mk.listings[0]['status'] == 1}, winner user {mk.listings[0]['high']} at ${mk.listings[0]['price'] // 100}.{mk.listings[0]['price'] % 100:02d}")

# ---- 6. JSON ----
print("-- 6. every JSON body the kernel printed, parsed with Python's json module")
bodies = []
for line in ser.splitlines():
    s = line.strip()
    if s.startswith("-> {"): bodies.append(("request", json.loads(s[3:])))
    elif s.startswith("{") and s.endswith("}") and '"' in s and not s.startswith("{}"):
        try: bodies.append(("response", json.loads(s)))
        except Exception as e: report(False, "response body is not valid JSON: " + s[:80] + " -- " + str(e))
report(len(bodies) > 15, f"{len(bodies)} JSON bodies parsed (offer request, bid and bidding responses, item view, orders, error objects)")
AMT = re.compile(r"^\d+\.\d\d$"); amount_ok = True; iso_ok = True
def walk(o, keys):
    global amount_ok, iso_ok
    if isinstance(o, dict):
        for k, v in o.items():
            keys.add(k)
            if k in ("currentPrice", "maxAmount", "total", "totalDueSeller", "amount", "minimumPriceToBid", "currentBidPrice", "auctionStartPrice", "auctionReservePrice") and isinstance(v, dict):
                amount_ok &= (set(v) == {"currency", "value"} and v["currency"] == "USD" and bool(AMT.match(v["value"])))
            if k in ("auctionEndDate", "itemEndDate"): iso_ok &= bool(re.fullmatch(r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z", v)) and v == "2026-10-11T00:00:00.000Z"
            walk(v, keys)
    elif isinstance(o, list):
        for x in o: walk(x, keys)
used = set()
for _, b in bodies: walk(b, used)
report(amount_ok, "every Amount is { currency: \"USD\", value: <string with exactly two decimals> }, as eBay's Amount type has it (value is a STRING)")
report(iso_ok, "every auctionEndDate / itemEndDate is ISO-8601 and equals the epoch plus the 1-day listing duration (2026-10-11T00:00:00.000Z)")
oas = set()
if oas_dir:
    specs = os.path.join(oas_dir, "src", "types", "restful", "specs")
    for f in ("buy_offer_v1_beta_oas3.ts", "buy_browse_v1_oas3.ts", "sell_inventory_v1_oas3.ts", "sell_fulfillment_v1_oas3.ts"):
        for mm in re.finditer(r"^\s{12}(\w+)\??:", open(os.path.join(specs, f)).read(), re.M): oas.add(mm.group(1))
    wrapper = {"status", "errors"}   # used by this chapter as the HTTP-style status echo and the {"errors":[...]} wrapper around eBay's Error object: those NAMES occur in the specs (as fields of other schemas), but this USE of them is the chapter's own addition and is not something a name check can confirm
    missing = sorted(used - oas)
    report(not missing, f"{len(used)} distinct field names used in the printed bodies; every one is a field name that occurs in eBay's real OpenAPI schemas (the {len(oas)} distinct names read from the four spec files); names in neither: {missing}; names this chapter uses in its OWN way (not confirmable by name): {sorted(used & wrapper)}")
else:
    print("  (skipped: pass --oas DIR to compare field names with eBay's OpenAPI files)")
print("all checks pass" if not fails else f"{fails} CHECK(S) FAILED"); sys.exit(1 if fails else 0)
