/* Chapter 5: this book's first real hardware interrupt setup. Every
 * chapter so far has run with interrupts disabled -- Chapter 4 gave the
 * CPU somewhere to go for a *self-inflicted* exception (#DE), but
 * nothing yet lets an actual external device, like a keyboard, ever
 * interrupt this kernel. That requires two more real pieces of
 * hardware to be configured correctly: the two 8259 Programmable
 * Interrupt Controllers (PICs) that every IRQ line physically routes
 * through before it ever reaches the CPU, and the CPU's own interrupt
 * flag (IF), which this chapter is the first to ever set.
 *
 * The PICs need attention before anything else, because their
 * out-of-the-box configuration is actively hostile to a protected-mode
 * kernel. By default, "the master PIC uses vector offset 0x08" and the
 * slave uses "0x70" -- real-mode-era defaults from when x86 had no
 * reserved exception range to collide with. In protected mode, that is
 * a real, direct conflict: "the IRQs 0 to 7 conflict with the CPU
 * exception which are reserved by Intel up until 0x1F" (OSDev Wiki,
 * "8259 PIC": https://wiki.osdev.org/8259_PIC). Concretely: with no
 * remap, IRQ1 (the keyboard) would fire on vector 0x09 -- which Intel
 * reserves for the Coprocessor Segment Overrun exception. This chapter
 * moves both PICs off that collision course before enabling anything. */

#include <stdint.h>

#include "012_pic.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Master/slave PIC command and data ports (OSDev Wiki, "8259 PIC"). */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define ICW1_ICW4  0x01 /* "Indicates that ICW4 will be present" */
#define ICW1_INIT  0x10 /* "Initialization - required!" */
#define ICW4_8086  0x01 /* "8086/88 (MCS-80/85) mode" */
#define CASCADE_IRQ 2

#define PIC_EOI 0x20

/* "Wait a very small amount of time (1 to 4 microseconds, generally)
 * ... You can do an IO operation on any unused port: the Linux kernel
 * by default uses port 0x80" -- real hardware delay for PICs old
 * enough to need one between successive initialization writes.
 * (OSDev Wiki, "Inline Assembly/Examples") */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* Quoted directly, structurally, from the OSDev Wiki's own PIC_remap
 * reference implementation (OSDev Wiki, "8259 PIC"): four real
 * Initialization Command Words (ICW1-ICW4) sent to each PIC in turn,
 * moving their vector offsets to offset1 (master) and offset2 (slave)
 * instead of the colliding real-mode defaults. */
void pic_remap(int offset1, int offset2) {
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, (uint8_t) offset1);
    io_wait();
    outb(PIC2_DATA, (uint8_t) offset2);
    io_wait();
    outb(PIC1_DATA, 1 << CASCADE_IRQ);
    io_wait();
    outb(PIC2_DATA, CASCADE_IRQ);
    io_wait();
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* The cited reference function ends here by writing 0 to both data
     * ports, which sets every IRQ line's mask bit to 0 -- unmasked,
     * meaning every one of the 15 usable IRQ lines is immediately
     * enabled. This kernel does not do that: it has a real handler for
     * exactly one line, IRQ1, and nothing installed for the other 14
     * -- including IRQ0, the timer, which fires on its own roughly 18
     * times a second whether or not anything is listening. Leaving
     * every line unmasked here would let IRQ0 fire into IDT vector
     * 0x20, an entry idt_init() never set the Present bit on, which
     * means an immediate triple fault the moment interrupts are
     * enabled. So this function stops short of that unmask step;
     * pic_disable_all() below puts the mask register into a known,
     * fully-masked state instead, and pic_clear_mask() is used
     * explicitly afterward, one line at a time, only for a line that
     * actually has a handler ready. */
}

void pic_disable_all(void) {
    /* The two data-port writes ICW4_8086 above just made are not, on
     * their own, a deliberate mask configuration -- they are leftover
     * values from the initialization sequence itself. Writing 0xFF to
     * both data ports here is what actually puts every one of the 15
     * usable IRQ lines into a known, fully-masked state before this
     * kernel selectively re-enables the one line it has a real
     * handler for. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

static uint16_t pic_get_mask(void) {
    return (uint16_t) (inb(PIC1_DATA) | ((uint16_t) inb(PIC2_DATA) << 8));
}

static void pic_write_mask(uint16_t mask) {
    outb(PIC1_DATA, (uint8_t) (mask & 0xFF));
    outb(PIC2_DATA, (uint8_t) ((mask >> 8) & 0xFF));
}

void pic_set_mask(uint8_t irq_line) {
    pic_write_mask((uint16_t) (pic_get_mask() | (1 << irq_line)));
}

void pic_clear_mask(uint8_t irq_line) {
    pic_write_mask((uint16_t) (pic_get_mask() & ~(1 << irq_line)));
}

void pic_send_eoi(uint8_t irq_line) {
    /* "For master-originated IRQs, write to the master command port
     * only; for slave IRQs, it is necessary to issue the command to
     * both PIC chips." (OSDev Wiki, "8259 PIC") IRQ1 is a master-only
     * line, so this chapter's own code path never takes the second
     * branch -- kept here so this function is correct for any future
     * chapter that enables an IRQ line 8-15. */
    if (irq_line >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}
