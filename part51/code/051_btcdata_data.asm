; Chapter 50: the blocks and transaction vectors the demo reads, baked into the kernel image with NASM's incbin (paths relative to the chapter's code directory, where build.sh runs). Made by make_btc_data.py (see data/btc/MANIFEST.txt).
BITS 32
section .rodata
%macro SAMPLE 2
global sample_%1_start
global sample_%1_end
sample_%1_start: incbin %2
sample_%1_end:
%endmacro
SAMPLE b_tn_0, "data/btc/tn_0.blk"
SAMPLE b_tn_2, "data/btc/tn_2.blk"
SAMPLE b_tn_3, "data/btc/tn_3.blk"
SAMPLE b_tn_15007, "data/btc/tn_15007.blk"
SAMPLE b_tn_49291, "data/btc/tn_49291.blk"
SAMPLE b_tn_180480, "data/btc/tn_180480.blk"
SAMPLE b_tn_926485, "data/btc/tn_926485.blk"
SAMPLE b_tn_987876, "data/btc/tn_987876.blk"
SAMPLE b_tn_1263442, "data/btc/tn_1263442.blk"
SAMPLE b_tn_1414221, "data/btc/tn_1414221.blk"
SAMPLE b_mn_0, "data/btc/mn_0.blk"
SAMPLE b_syn_1, "data/btc/syn_1.blk"
SAMPLE b_syn_2, "data/btc/syn_2.blk"
SAMPLE b_syn_3, "data/btc/syn_3.blk"
SAMPLE b_syn_4, "data/btc/syn_4.blk"
SAMPLE b_syn_5, "data/btc/syn_5.blk"
SAMPLE b_syn_6, "data/btc/syn_6.blk"
SAMPLE b_syn_7, "data/btc/syn_7.blk"
SAMPLE b_syn_8, "data/btc/syn_8.blk"
SAMPLE b_syn_9, "data/btc/syn_9.blk"
SAMPLE b_syn_10, "data/btc/syn_10.blk"
SAMPLE b_syn_11, "data/btc/syn_11.blk"
SAMPLE b_syn_12, "data/btc/syn_12.blk"
SAMPLE b_syn_dup, "data/btc/syn_dup.blk"
SAMPLE txvec, "data/btc/txvec.bin"
