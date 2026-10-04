"""Scenes for chapters 1-23 (booting, screen, tables, interrupts, input, time, memory, scheduling, locks, protection, loading, disk and filesystem). Each function takes an Svg and draws onto it."""
import math
def s01(s):  # boot: power button, firmware arrow, chip, kernel entry
    s.circ(120, 150, 52, s.dark, s.acc, 5); s.path("M120,118 v30", stroke=s.acc, sw=7); s.path("M98,132 a30,30 0 1 0 44,0", stroke=s.acc, sw=7)
    s.arrow(190, 150, 300, 150); s.rect(310, 105, 110, 90, s.dark, s.light, 3); s.text(365, 140, "GRUB", 20, s.light, bold=True); s.text(365, 170, "multiboot2", 12, s.main)
    s.arrow(430, 150, 520, 150); s.chip(540, 112, 76, "CPU"); s.text(578, 230, "_start", 18, s.acc, bold=True); s.text(578, 255, "0x100000", 12, s.light, op=.7)
def s02(s):  # VGA text mode
    s.monitor(80, 60, 300, 190, "#050a14")
    cols = ["#f87171", "#fbbf24", "#34d399", "#60a5fa", "#c084fc", "#f9fafb"]
    for r in range(8):
        for c in range(18): s.rect(98 + c * 15.4, 76 + r * 18.4, 11, 13, cols[(r * 3 + c) % 6], None, 0, 1, op=.85 if (r * 7 + c) % 5 else .25)
    s.text(550, 120, "0xB8000", 28, s.acc, bold=True); s.rect(470, 140, 160, 46, s.dark, s.main, 2); s.text(500, 170, "'A'", 20, s.light); s.rect(530, 150, 40, 26, s.main); s.text(550, 169, "0x1F", 14, s.dark, bold=True); s.text(550, 220, "char + attribute", 14, s.light, op=.8)
def s03(s):  # GDT + printf
    rows = [("0", "null"), ("1", "kernel code"), ("2", "kernel data")]
    s.rect(70, 50, 330, 200, s.dark, s.main, 3)
    for k, (n, t) in enumerate(rows): s.rect(84, 64 + k * 60, 302, 48, "#0e2036" if k else "#1b0f0f", s.light, 1.5, 4); s.text(110, 94 + k * 60, n, 22, s.acc, bold=True); s.text(250, 94 + k * 60, t, 18, s.light)
    s.text(235, 40, "GDT", 18, s.main, bold=True); s.arrow(410, 150, 480, 150); s.text(600, 120, 'printf("%d")', 22, s.acc, bold=True); s.rect(500, 140, 200, 56, s.dark, s.light, 2); s.text(600, 176, "-> 42", 22, s.light, bold=True)
def s04(s):  # interrupts: bell
    s.path("M170,210 q-5,-30 0,-70 a50,50 0 0 1 100,0 q5,40 0,70 z", fill=s.acc, stroke=s.dark, sw=3); s.circ(220, 224, 10, s.acc, s.dark, 3); s.rect(212, 78, 16, 14, s.acc, s.dark, 2, 3)
    for k in range(3): s.path(f"M{300 + k * 26},{150 - 20 - k * 8} q{14 + k * 4},{20 + k * 8} 0,{40 + k * 16}", stroke=s.main, sw=4, op=.9 - k * .2)
    s.chip(520, 112, 76, "CPU"); s.text(558, 230, "IDT[32]", 18, s.acc, bold=True); s.arrow(440, 150, 505, 150)
def s05(s):  # keyboard
    for r, n in enumerate((10, 10, 9)):
        for c in range(n): s.rect(70 + r * 18 + c * 44, 70 + r * 50, 38, 40, s.light if (r + c) % 4 else s.acc, s.dark, 2, 6, op=.95)
    s.rect(130, 226, 320, 32, s.light, s.dark, 2, 6); s.text(650, 120, "IRQ 1", 28, s.acc, bold=True); s.text(650, 160, "port 0x60", 18, s.light); s.text(650, 195, "scancode 0x1E", 16, s.main)
def s06(s):  # PIT timer: clock
    s.circ(200, 150, 100, s.dark, s.light, 6)
    for k in range(12): a = k * math.pi / 6; s.line(200 + 84 * math.sin(a), 150 - 84 * math.cos(a), 200 + 94 * math.sin(a), 150 - 94 * math.cos(a), s.acc if k % 3 == 0 else s.light, 4)
    s.line(200, 150, 200 + 60 * math.sin(2.2), 150 - 60 * math.cos(2.2), s.light, 6); s.line(200, 150, 200 + 80 * math.sin(.6), 150 - 80 * math.cos(.6), s.main, 4); s.circ(200, 150, 8, s.acc)
    for k in range(7): s.rect(380 + k * 48, 150 - (20 if k % 2 else 50), 22, 20 if k % 2 else 50, s.main if k % 2 else s.acc, None, 0, 2)
    s.line(370, 152, 720, 152, s.light, 2); s.text(545, 215, "tick tick tick", 18, s.light); s.text(545, 245, "100 Hz", 22, s.acc, bold=True)
def s07(s):  # physical memory: frames
    s.grid(70, 60, 16, 6, 30, 30, 4, lambda r, c: s.acc if (r * 16 + c) in (3, 4, 5, 20, 21, 40, 41, 42, 43, 70) else (s.main if (r * 16 + c) % 7 else s.light), s.dark)
    s.text(70, 285, "4 KiB frames", 16, s.light, "start"); s.text(730, 285, "bitmap: 1 = used", 16, s.acc, "end"); s.text(70, 44, "0x0", 14, s.light, "start", op=.7); s.text(730, 44, "RAM", 14, s.light, "end", op=.7)
def s08(s):  # paging
    s.text(150, 52, "virtual", 16, s.acc); s.text(620, 52, "physical", 16, s.acc)
    pairs = ((0, 2), (1, 0), (2, 3), (3, 1))
    for v, p in pairs: s.rect(90, 70 + v * 52, 120, 40, s.main, s.dark, 2, 4); s.text(150, 96 + v * 52, f"page {v}", 16, s.dark, bold=True); s.rect(560, 70 + p * 52, 120, 40, s.light, s.dark, 2, 4); s.text(620, 96 + p * 52, f"frame {p}", 16, s.dark, bold=True); s.arrow(220, 90 + v * 52, 550, 90 + p * 52, s.acc, 3)
    s.rect(330, 120, 120, 70, s.dark, s.light, 2); s.text(390, 152, "CR3", 20, s.acc, bold=True); s.text(390, 176, "page dir", 13, s.light)
def s09(s):  # heap
    xs = [(60, 90, 0), (150, 60, 1), (210, 130, 0), (340, 70, 1), (410, 110, 0), (520, 80, 1), (600, 120, 0)]
    for x, w, free in xs: s.rect(x, 120, w - 4, 70, s.light if free else s.main, s.dark, 2, 3); s.text(x + w / 2 - 2, 162, "free" if free else "used", 14, s.dark, bold=True)
    s.text(100, 90, "kmalloc(64)", 20, s.acc, "start", bold=True); s.arrow(250, 96, 250, 112); s.text(470, 90, "kfree(p)", 20, s.acc, "start", bold=True); s.arrow(470, 96, 470, 112, s.light); s.text(400, 240, "header | payload | next", 16, s.light, op=.8)
def s10(s):  # page fault
    s.poly([(220, 60), (330, 240), (110, 240)], s.acc, s.dark, 4); s.text(220, 215, "!", 100, s.dark, bold=True); s.rect(420, 90, 120, 120, s.dark, s.main, 3); s.text(480, 160, "#PF", 36, s.acc, bold=True)
    s.path("M420,150 q-30,-40 -70,-10", stroke=s.light, sw=3); s.rect(580, 110, 150, 80, s.dark, s.light, 2); s.text(655, 140, "CR2", 18, s.acc, bold=True); s.text(655, 170, "0xDEAD000", 14, s.light)
def s11(s):  # cooperative tasks: relay baton
    for k in range(4): x = 110 + k * 170; s.circ(x, 150, 38, s.dark, s.main, 4); s.text(x, 158, f"T{k + 1}", 24, s.light, bold=True)
    for k in range(3): s.arrow(150 + k * 170, 150, 242 + k * 170 - 4, 150, s.acc, 4)
    s.text(400, 240, "yield()", 28, s.acc, bold=True); s.rect(300, 40, 200, 30, s.main, None, 0, 15); s.text(400, 62, "one at a time", 14, s.dark, bold=True)
def s12(s):  # preemptive: clock + round robin
    s.circ(400, 150, 80, s.dark, s.main, 4); s.text(400, 142, "timer", 18, s.acc, bold=True); s.text(400, 168, "IRQ 0", 16, s.light)
    for k in range(4): a = k * math.pi / 2 - math.pi / 4; x = 400 + 150 * math.cos(a); y = 150 + 120 * math.sin(a); s.circ(x, y, 30, s.main, s.dark, 3); s.text(x, y + 7, f"T{k + 1}", 20, s.dark, bold=True)
    s.path("M400,50 a100,100 0 0 1 100,100", stroke=s.acc, sw=4); s.poly([(500, 150), (492, 134), (508, 134)], s.acc)
def s13(s):  # spinlock
    s.lockicon(330, 70, 2.4); s.path("M520,150 a50,50 0 1 1 -10,-30", stroke=s.acc, sw=6); s.poly([(500, 110), (520, 116), (514, 96)], s.acc)
    s.text(580, 100, "while(locked)", 16, s.light, "start"); s.text(580, 124, "  spin;", 16, s.acc, "start"); s.text(400, 270, "xchg", 28, s.main, bold=True); s.circ(170, 150, 38, s.main, s.dark, 3); s.text(170, 158, "CPU", 18, s.dark, bold=True)
def s14(s):  # semaphores: flag + sleeping tasks
    s.line(130, 60, 130, 250, s.light, 6); s.poly([(136, 66), (260, 100), (136, 134)], s.acc, s.dark, 3); s.text(200, 106, "P()", 18, s.dark, bold=True)
    for k in range(3): s.circ(380 + k * 90, 190, 30, s.dark, s.main, 3); s.text(380 + k * 90, 198, "zzz", 18, s.light)
    s.text(470, 120, "wait queue", 20, s.acc, bold=True); s.text(470, 250, "sleep / wake", 16, s.light); s.circ(650, 100, 24, s.acc, s.dark, 3); s.text(650, 109, "V", 22, s.dark, bold=True)
def s15(s):  # rings
    for k, (r, c) in enumerate(((120, s.main), (84, s.acc), (48, s.light))): s.circ(250, 150, r, c, s.dark, 3, op=.35 + k * .2)
    s.text(250, 158, "ring 0", 16, s.dark, bold=True); s.text(250, 255, "ring 3", 16, s.light); s.arrow(380, 150, 480, 150, s.acc); s.rect(500, 100, 200, 100, s.dark, s.light, 2); s.text(600, 140, "int 0x80", 22, s.acc, bold=True); s.text(600, 172, "syscall", 16, s.light)
def s16(s):  # ring 3 tasks around the kernel
    s.circ(170, 150, 100, s.main, s.dark, 3, op=.3); s.text(170, 158, "kernel", 18, s.light, bold=True)
    for k, (x, y) in enumerate(((380, 50), (540, 50), (460, 130))): s.rect(x, y, 130, 56, s.dark, s.acc, 3); s.text(x + 65, y + 34, f"user {k + 1}", 16, s.light); s.arrow(280 if k < 2 else 290, 150, x - 4, y + 28, s.main, 2)
    s.text(500, 250, "sys_yield    sys_exit", 20, s.acc, bold=True)
def s17(s):  # per-process address spaces
    for k in range(3):
        x = 70 + k * 235; s.rect(x, 60, 200, 150, s.dark, s.main, 3); s.text(x + 100, 48, f"process {k + 1}", 15, s.light)
        for r in range(4): s.rect(x + 14, 74 + r * 32, 172, 24, s.acc if r == k else s.main, s.dark, 1.5, 3, op=.9)
        s.rect(x + 40, 226, 120, 34, s.light, s.dark, 2); s.text(x + 100, 249, f"CR3 = pd{k + 1}", 14, s.dark, bold=True)
def s18(s):  # ELF
    s.rect(70, 50, 150, 200, s.dark, s.light, 3); s.text(145, 78, "ELF", 22, s.acc, bold=True)
    for k, (t, c) in enumerate((("header", s.main), (".text", s.acc), (".data", s.light), (".bss", s.main))): s.rect(84, 92 + k * 38, 122, 30, c, s.dark, 2, 3); s.text(145, 113 + k * 38, t, 14, s.dark, bold=True)
    s.arrow(230, 150, 420, 150); s.text(325, 130, "PT_LOAD", 16, s.light); s.grid(440, 70, 6, 5, 40, 32, 4, lambda r, c: s.acc if (r, c) in ((1, 1), (1, 2), (2, 1)) else s.main); s.text(580, 270, "0x400000", 16, s.acc)
def s19(s):  # ATA disk
    s.circ(250, 150, 110, "#1b1f27", s.light, 4); s.circ(250, 150, 80, None, s.main, 2, op=.5); s.circ(250, 150, 50, None, s.main, 2, op=.5); s.circ(250, 150, 14, s.light, s.dark, 3)
    s.path("M380,60 L270,140", stroke=s.acc, sw=9); s.circ(384, 56, 12, s.acc, s.dark, 3); s.text(570, 105, "LBA 0", 22, s.acc, bold=True); s.text(570, 140, "port 0x1F0", 18, s.light); s.text(570, 175, "512-byte sectors", 16, s.main); s.text(570, 215, "inw()  outw()", 16, s.light, op=.8)
def s20(s):  # FAT16 chain
    s.text(110, 50, "FAT", 20, s.acc, bold=True)
    for k, v in enumerate(("0003", "0004", "0007", "FFFF")): s.rect(70 + k * 92, 62, 80, 40, s.dark, s.main, 2); s.text(110 + k * 92, 88, v, 18, s.light)
    for k, (cl, c) in enumerate(((3, s.acc), (4, s.acc), (7, s.acc), (5, s.main))): pass
    for k, cl in enumerate((3, 4, 7)): s.rect(120 + k * 160, 170, 120, 56, s.light, s.dark, 2, 4); s.text(180 + k * 160, 205, f"cluster {cl}", 16, s.dark, bold=True)
    s.arrow(240, 198, 278, 198, s.acc); s.arrow(400, 198, 438, 198, s.acc); s.file(660, 150, 56, 70); s.text(688, 250, "README.TXT", 12, s.light)
def s21(s):  # subdirectories
    s.folder(80, 80, 110, 80, s.acc, "ROOT"); s.line(190, 110, 290, 70, s.light); s.line(190, 130, 290, 150); s.line(190, 150, 290, 230)
    s.folder(290, 45, 100, 60, s.main, "DOCS"); s.folder(290, 125, 100, 60, s.main, "BIN"); s.file(310, 205, 40, 52, "A.TXT"); s.line(390, 75, 470, 75); s.file(470, 52, 40, 50, "N.TXT")
def s22(s):  # rmdir
    s.folder(110, 90, 150, 110, s.acc, "EMPTY"); s.line(300, 100, 380, 190, s.light, 12); s.line(380, 100, 300, 190, s.light, 12); s.line(300, 100, 380, 190, "#ef4444", 8); s.line(380, 100, 300, 190, "#ef4444", 8)
    s.text(580, 120, "rmdir()", 28, s.acc, bold=True); s.text(580, 160, "only if empty", 18, s.light); s.text(580, 195, "'.' and '..' only", 14, s.main)
def s23(s):  # recursive paths
    s.text(400, 46, "/A/B/C/D/FILE.TXT", 26, s.acc, bold=True)
    for k, n in enumerate(("/", "A", "B", "C", "D")): s.folder(50 + k * 130, 90 + k * 30, 90, 60, s.main if k else s.acc, n)
    for k in range(4): s.arrow(140 + k * 130, 118 + k * 30, 176 + k * 130, 130 + k * 30, s.light, 2)
    s.arrow(660, 240, 700, 244, s.light, 2); s.file(705, 215, 44, 56, "FILE.TXT")
