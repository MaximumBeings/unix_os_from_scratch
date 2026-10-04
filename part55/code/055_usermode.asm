; This chapter's real privilege-level transition -- the one instruction
; that actually changes CPL from 0 to 3 is IRET, executed here with a
; stack this routine builds BY HAND to make the CPU believe it is
; returning from an interrupt that originally came from ring 3 (OSDev
; Wiki, "Getting to Ring 3": "make the processor think it was already in
; ring 3 to start with"). Because this is a real privilege INCREASE in
; number (0 -> 3, a decrease in actual privilege), IRET pops five real
; values, not three: EIP, CS, EFLAGS, and then -- because it detects the
; popped CS's own RPL (3) is numerically greater than the CPL it is
; currently running at (0) -- ESP and SS as well, switching stacks in
; the same instruction. cdecl: entry point in [esp+4], the new ring-3
; stack's own top address in [esp+8].
BITS 32

section .text
global enter_usermode
enter_usermode:
    mov eax, [esp + 4]      ; entry point this ring-3 task starts at
    mov ecx, [esp + 8]      ; top of the ring-3 stack it starts with

    ; ds/es/fs/gs are NOT among the five values IRET pops below -- only
    ; cs and ss are ever reloaded by a privilege-changing IRET. Reload
    ; the other four by hand first, to the user data selector (GDT
    ; index 4, 4*8=0x20, RPL 3 -> 0x23), or every ordinary data access
    ; this task makes right after entry would still be reading through
    ; this kernel's own RING-0 data selector.
    mov dx, 0x23
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    push 0x23                ; SS: user data selector, RPL 3
    push ecx                  ; ESP: this task's own new ring-3 stack top

    pushfd                     ; start from the CURRENT real EFLAGS...
    pop edx
    or edx, 0x200                ; ...and force IF=1 in the COPY that is
                                   ; about to be pushed, so this task keeps
                                   ; receiving real timer/keyboard
                                   ; interrupts once it is actually
                                   ; running in ring 3 -- IRET loads
                                   ; EFLAGS from the stack, not from the
                                   ; CPU's live copy, so this is the only
                                   ; place that matters
    push edx

    push 0x1B                    ; CS: user code selector (GDT index 3,
                                   ; 3*8=0x18, RPL 3 -> 0x1B)
    push eax                      ; EIP: this task's own real entry point

    iret                          ; pops EIP, CS, EFLAGS, ESP, SS, in
                                    ; that order -- the real ring 0 -> 3
                                    ; transition happens on this exact
                                    ; instruction, not before it
