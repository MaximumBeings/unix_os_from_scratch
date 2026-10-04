; Chapter 48: six real SEC filings baked into the kernel image. Each file is the slimmed XBRL instance document made by edgar_slim.py (byte-for-byte copies of the filer's own contexts,
; units and facts; see the page). NASM's incbin copies the file's bytes into .rodata at assembly time -- no generated C source, no byte-array listing. The path is relative to the directory
; build.sh runs in (the chapter's code directory).
BITS 32
section .rodata
%macro FILING 2
global filing_%1_start
global filing_%1_end
filing_%1_start: incbin %2
filing_%1_end:
%endmacro
FILING aapl, "data/aapl.xml"
FILING ko, "data/ko.xml"
FILING nvda, "data/nvda.xml"
FILING msft, "data/msft.xml"
FILING xom, "data/xom.xml"
FILING jpm, "data/jpm.xml"
