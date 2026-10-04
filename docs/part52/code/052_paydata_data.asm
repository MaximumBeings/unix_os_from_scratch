; Chapter 52: the invented company's year of payroll, baked into the kernel image with NASM's incbin (path relative to the chapter's code directory). Made by make_payroll.py.
BITS 32
section .rodata
global pay_year_start, pay_year_end
pay_year_start: incbin "data/pay/year.txt"
pay_year_end:
