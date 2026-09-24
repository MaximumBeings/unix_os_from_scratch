; Chapter 11: the one real context switch this whole book comes down
; to. Every C function on this book's freestanding i686 target already
; uses cdecl, and cdecl's own convention is what makes this routine as
; small as it is: "EAX, ECX, and EDX are already saved by the caller
; and don't need to be saved again" (OSDev Wiki, "Kernel Multitasking":
; https://wiki.osdev.org/Kernel_Multitasking) -- whichever C code
; called switch_task() already assumed those three could be clobbered
; by any ordinary function call, switch_task() included. That leaves
; exactly four registers this routine has to save by hand: EBX, ESI,
; EDI, EBP. EIP does not need to be saved explicitly either -- `call`
; already pushed the caller's own return address, and the matching
; `ret` at the end of this routine is what actually resumes the *new*
; task, by popping whatever return address sits on top of *its* stack.
;
; void switch_task(uint32_t *old_esp_ptr, uint32_t new_esp);
;   old_esp_ptr -- where to store this task's own ESP, once every
;                  register this routine owns has been pushed
;   new_esp     -- the ESP to switch onto: some other task's own
;                  previously-saved ESP (task_yield() resuming it), or
;                  a stack task_create() built by hand (task_switch()
;                  running it for the very first time)
BITS 32

section .text
global switch_task
switch_task:
    push ebp
    push ebx
    push esi
    push edi
    ; four pushes above (16 bytes) sit on top of the return address
    ; `call` pushed (4 bytes) and this routine's own two cdecl
    ; arguments above that -- so old_esp_ptr is 16+4=20 bytes above
    ; the current ESP, and new_esp is 24 bytes above it.
    mov eax, [esp+20]      ; eax = old_esp_ptr
    mov [eax], esp          ; *old_esp_ptr = esp (this task's saved state)

    mov eax, [esp+24]      ; eax = new_esp
    mov esp, eax             ; the actual switch: every register below
                              ; this point is read from the NEW task's
                              ; own stack, not the old one's

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                        ; pops whatever return address sits on
                                 ; top of the new stack -- either back
                                 ; into task_yield() for a task resuming
                                 ; where it left off, or straight into
                                 ; task_create()'s chosen entry point,
                                 ; the first time this task ever runs
