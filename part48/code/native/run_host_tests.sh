#!/bin/bash
# Chapter 48: every host-side test, the C built with AddressSanitizer and UBSan, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"
echo "== 1. the XBRL reader and the ratio engine: arithmetic, parsers, hand-computed answers, malformed documents =="
gcc $F xbrl_test.c ../048_xbrl.c ../048_ratios.c ../048_filings.c -o /tmp/c48_xt && /tmp/c48_xt | grep -v "^PASS" ; /tmp/c48_xt | grep -c "^PASS" | sed 's/$/ lines PASS/'
echo; echo "== 2. the six real filings: C engine (the kernel's own source files) vs the independent Python reference, price \$100.00 =="
gcc $F ratios_cli.c ../048_xbrl.c ../048_ratios.c -o /tmp/c48_cli
for t in aapl ko nvda msft xom jpm; do
  if cmp -s <(/tmp/c48_cli ../data/$t.xml 10000) <(python3 ../ratios_ref.py ../data/$t.xml 10000); then echo "$t: C output and Python output IDENTICAL ($(/tmp/c48_cli ../data/$t.xml 10000 | wc -l) lines)"; else echo "$t: DIFFERENT"; fi
done
echo; echo "== 3. 3000 random valid filings (gen_docs.py): C engine vs independent Python reference =="
python3 diff_test.py 3000 /tmp/c48_cli
echo; echo "== 4. the EDGAR fetcher against a local server speaking the SEC's JSON shapes =="
python3 test_fetch.py
echo; echo "== 5. damaged filings: 5,000 mutations of each of the six real filings, sanitizers on =="
gcc $F xbrl_fuzz.c ../048_xbrl.c ../048_ratios.c -o /tmp/c48_fz && /tmp/c48_fz 5000 ../data/*.xml
