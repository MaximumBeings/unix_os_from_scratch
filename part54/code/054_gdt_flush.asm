; Chapter 5: loading a new GDT is two real steps, not one. LGDT alone only
; tells the CPU where the table lives -- it does not, by itself, change
; anything the CPU is currently using. Every segment register still holds
; whatever selector it held before, and those old selectors keep working
; only by coincidence, if the new table happens to define compatible
; descriptors at the same indices. This routine makes the switch real:
; load the table, then explicitly reload every segment register,
; including CS, which cannot be reloaded with a plain MOV (OSDev Wiki,
; "GDT Tutorial": "changing the CS register requires code resembling a
; jump or call to elsewhere, as this is the only way its value is meant
; to be changed").
BITS 32

section .text
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]     ; cdecl: the one argument (a gdt_ptr*) is on
                            ; the stack, 4 bytes above the return address
    lgdt [eax]              ; load GDTR from the gdt_ptr struct this
                            ; points at (2-byte limit, 4-byte base)

    mov ax, 0x10            ; kernel data selector: GDT index 2, and
                            ; 2 * 8 bytes-per-entry = 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.reload_cs     ; far jump to kernel code selector (index 1,
                            ; 1 * 8 = 0x08) -- the only way to reload CS
.reload_cs:
    ret
