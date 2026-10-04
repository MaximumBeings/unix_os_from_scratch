#!/usr/bin/env python3
"""Chapter 47: checks the ISO-8601 times printed by ebay_test (lines "ISO <seconds> <time>") against Python's own datetime, which shares no code with 047_ebay.c's date algorithm.
Usage: ebay_iso_check.py ebay_out.txt"""
import sys, datetime
base = datetime.datetime(2026, 10, 10, tzinfo=datetime.timezone.utc); n = bad = 0
for line in open(sys.argv[1]):
    if line.startswith("ISO "):
        _, secs, shown = line.split(); want = (base + datetime.timedelta(seconds=int(secs))).strftime("%Y-%m-%dT%H:%M:%S.000Z"); n += 1
        if shown != want: bad += 1; print("DIFF", secs, shown, want)
print(f"{n} instants checked against datetime: {'all identical' if not bad else str(bad) + ' differ'}")
sys.exit(1 if bad else 0)
