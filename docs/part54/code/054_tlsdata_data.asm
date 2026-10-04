; Chapter 54: the recorded handshake of RFC 8448 section 3 (as in the Botan test data), baked into the kernel image with NASM's incbin (path relative to the chapter's code directory). Made by make_tls_data.py.
BITS 32
section .rodata
global tls_trace_start, tls_trace_end
tls_trace_start: incbin "data/tls/rfc8448_simple.txt"
tls_trace_end:
