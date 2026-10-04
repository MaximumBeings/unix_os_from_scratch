; Chapter 53: the scripted warehouse workload, baked into the kernel image with NASM's incbin (path relative to the chapter's code directory). Made by make_kv.py.
BITS 32
section .rodata
global kv_demo_start, kv_demo_end
kv_demo_start: incbin "data/kv/demo.txt"
kv_demo_end:
