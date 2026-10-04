#!/bin/bash
# Chapter 54: every host-side test, the C built with AddressSanitizer and UBSan, in one run. Writes everything to stdout (the chapter page shows it as host_tests_out.txt).
cd "$(dirname "$0")"; F="-O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"; SRC="../054_tls.c ../054_sha256.c ../054_hmac.c ../054_aes.c"; VEC=../data/tls/botan_rfc8448_transcripts.vec
echo "== 1. the primitives and the API, from RFC 7748, RFC 5869 and the GCM specification =="
gcc $F tls_test.c $SRC -o /tmp/c54_t 2>&1 | grep -i error; /tmp/c54_t | grep -v "^PASS" | grep -v "^$"; /tmp/c54_t | grep -c "^PASS" | sed 's/$/ lines PASS/'
echo; echo "== 2. the recorded handshake of RFC 8448 section 3: every byte the client sends equals the record, every server message is accepted =="
gcc $F tls_trace.c $SRC -o /tmp/c54_tr && /tmp/c54_tr $VEC
echo; echo "== 3. the primitives against an independent library (Python cryptography) =="
gcc $F tls_prim.c $SRC -o /tmp/c54_prim; python3 diff_prims.py 60 /tmp/c54_prim
echo; echo "== 4. full handshakes against the independent Python server, the attack catalogue, every byte flipped =="
gcc $F tls_hs.c $SRC -o /tmp/c54_hs; python3 diff_tls.py 40 /tmp/c54_hs
echo; echo "== 5. mutation fuzzing of the recorded handshake =="
gcc $F tls_fuzz.c $SRC -o /tmp/c54_fz && /tmp/c54_fz $VEC 1500
