; Chapter 51: the two command streams the demo runs, baked into the kernel image with NASM's incbin (paths relative to the chapter's code directory). data/book/demo.txt is written by hand; data/book/flow.txt is generated (native/gen_flow.py 51 1200): SYNTHETIC order flow.
BITS 32
section .rodata
global book_demo_start, book_demo_end, book_flow_start, book_flow_end
book_demo_start: incbin "data/book/demo.txt"
book_demo_end:
book_flow_start: incbin "data/book/flow.txt"
book_flow_end:
