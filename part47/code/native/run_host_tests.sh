#!/bin/sh
# Chapter 47: every host-side test, built with AddressSanitizer and UndefinedBehaviorSanitizer, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -I.."
SRC="../047_market.c ../047_auction.c ../047_ledger.c ../047_wal.c ../047_sha256.c"
echo "== auction engine vs the independent Python reference (20000 random auctions) =="
gcc $F auction_fuzz.c ../047_auction.c -o /tmp/c47_fuzz && /tmp/c47_fuzz 20000 > /tmp/c47_c.txt && python3 auction_ref.py 20000 > /tmp/c47_p.txt
if cmp -s /tmp/c47_c.txt /tmp/c47_p.txt; then echo "C engine output and Python reference output are IDENTICAL ($(wc -l < /tmp/c47_c.txt) lines)"; else echo "DIFFERENT"; diff /tmp/c47_c.txt /tmp/c47_p.txt | head -5; fi
python3 fuzz_stats.py /tmp/c47_c.txt
echo; echo "== marketplace: invariants, replay, every log prefix, torn tails, idempotency, fee split, hash sensitivity (600 random marketplaces) =="
gcc $F market_test.c $SRC -o /tmp/c47_mt && /tmp/c47_mt 600
echo; echo "== ledger: conservation, overdraft, fee table vs decimal arithmetic =="
gcc $F ledger_test.c ../047_ledger.c -o /tmp/c47_lt && /tmp/c47_lt > /tmp/c47_l.txt; grep -v "^FEE" /tmp/c47_l.txt; python3 ledger_ref.py /tmp/c47_l.txt
echo; echo "== eBay-shaped messages: round trips, strict amounts, builders, 2,000,000 damaged inputs =="
gcc $F ebay_test.c ../047_ebay.c $SRC -o /tmp/c47_et && /tmp/c47_et > /tmp/c47_e.txt; grep -v "^ISO" /tmp/c47_e.txt; python3 ebay_iso_check.py /tmp/c47_e.txt
