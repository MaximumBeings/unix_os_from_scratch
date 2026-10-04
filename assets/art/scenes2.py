"""Scenes for chapters 24-46 (PCI, network driver, ARP, IP, and the applied chapters: payments, insurance, investing, budgeting, streaming, ATM, POS, billing, travel, tickets, betting, rental) and the appendices."""
import math
def s24(s):  # PCI bus
    s.rect(60, 190, 680, 18, s.light, s.dark, 2, 3); s.text(400, 240, "config space: vendor 0x10EC  device 0x8139", 17, s.acc)
    for k, (t, c) in enumerate((("NIC", s.acc), ("GPU", s.main), ("IDE", s.light), ("???", s.main))): x = 110 + k * 160; s.rect(x, 80, 100, 100, s.dark, c, 4, 6); s.text(x + 50, 138, t, 24, c, bold=True); s.rect(x + 40, 180, 20, 12, c)
    s.text(110, 60, "bus 0", 16, s.light, "start")
def s25(s):  # RTL8139 NIC
    s.rect(90, 70, 300, 150, "#0e3b2e", s.main, 3, 8); s.chip(150, 105, 70, "8139"); s.rect(250, 120, 110, 40, s.dark, s.light, 2); s.text(305, 146, "RJ45", 16, s.light)
    for k in range(8): s.rect(100 + k * 14, 220, 8, 22, s.acc)
    s.line(390, 140, 470, 140, s.light, 6); s.packet(480, 118, 140, 46, "frame", s.main); s.arrow(630, 141, 710, 141); s.text(600, 215, "TX  /  RX", 22, s.acc, bold=True)
def s26(s):  # interrupt-driven NIC: moon (hlt) + bell + NIC
    s.path("M200,60 a80,80 0 1 0 60,130 a60,60 0 1 1 -60,-130 z", s.light, None); s.text(300, 90, "z z z", 28, s.light, "start", op=.8); s.text(230, 245, "hlt", 28, s.main, bold=True)
    s.arrow(330, 150, 460, 150, s.acc); s.rect(480, 100, 120, 100, s.dark, s.acc, 3); s.text(540, 150, "RTL", 22, s.acc, bold=True); s.text(540, 178, "IRQ 11", 14, s.light)
    s.path("M630,110 q30,40 0,80", stroke=s.main, sw=4); s.path("M655,95 q45,55 0,110", stroke=s.main, sw=3, op=.6)
def s27(s):  # multi-frame ring buffer
    for k in range(8): a = k * math.pi / 4; x = 400 + 100 * math.cos(a) - 28; y = 150 + 100 * math.sin(a) - 20; s.rect(x, y, 56, 40, s.acc if k == 5 else s.main, s.dark, 3, 5); s.text(x + 28, y + 26, f"{k}", 18, s.dark, bold=True)
    s.path("M470,60 a110,110 0 0 1 60,50", stroke=s.light, sw=3); s.poly([(530, 110), (518, 98), (535, 98)], s.light); s.text(400, 155, "CAPR", 22, s.acc, bold=True); s.text(130, 150, "wraps", 20, s.light); s.text(660, 150, "round-robin TX", 16, s.light, "start", op=.9)
def s28(s):  # ARP: who has?
    for x, y in ((200, 100), (200, 240), (640, 100), (640, 240)): s.line(x, y, 380, 100, s.acc, 2, "6 6")
    s.cloud(350, 100, 1.2, s.light)
    for k, (x, y) in enumerate(((110, 70), (110, 210), (640, 70), (640, 210), (380, 215))): s.rect(x, y, 90, 60, s.dark, s.light, 2, 6); s.text(x + 45, y + 36, f"10.0.0.{k + 1}", 14, s.light)
    s.text(365, 98, "Who has", 18, s.dark, bold=True); s.text(365, 122, "10.0.0.5 ?", 16, s.dark, bold=True); s.text(380, 290, "to FF:FF:FF:FF:FF:FF", 14, s.acc)
def s29(s):  # ARP cache
    s.rect(110, 60, 580, 40, s.main, s.dark, 2); s.text(250, 87, "IP address", 18, s.dark, bold=True); s.text(520, 87, "MAC address", 18, s.dark, bold=True)
    for k in range(4): y = 100 + k * 44; s.rect(110, y, 580, 40, s.dark if k % 2 else "#0b2a4a", s.light, 1.5); s.text(250, y + 27, f"10.0.0.{k + 2}", 17, s.light); s.text(520, y + 27, f"52:54:00:12:34:5{k}", 16, s.acc)
    s.text(400, 290, "merge_flag", 22, s.main, bold=True)
def s30(s):  # wire transfer: bank, lock, encrypted
    s.bank(60, 70, 1.15); s.bank(520, 70, 1.15); s.packet(290, 120, 180, 52, "$$$$", s.acc); s.arrow(236, 146, 284, 146, s.light, 3); s.arrow(474, 146, 516, 146, s.light, 3); s.lockicon(350, 190, 1.2)
    s.text(380, 275, "AES-128 · SHA-256 · HMAC", 16, s.light)
def s31(s):  # ARP server: server answering
    s.server(110, 80, 110, 140); s.text(165, 245, "10.0.0.2", 16, s.acc, bold=True); s.rect(300, 70, 190, 56, s.dark, s.main, 2, 28); s.text(395, 105, "who has .2?", 17, s.light); s.rect(300, 175, 190, 56, s.acc, s.dark, 2, 28); s.text(395, 210, ".2 is at 52:54", 16, s.dark, bold=True)
    s.arrow(300, 98, 235, 125, s.light, 2); s.arrow(300, 200, 235, 180, s.dark, 2); s.circ(640, 150, 50, s.dark, s.main, 3); s.text(640, 156, "ask", 22, s.main, bold=True)
def s32(s):  # ACH: split the bill
    s.rect(70, 100, 150, 110, s.light, s.dark, 3, 8); s.text(145, 140, "DINNER", 16, s.dark, bold=True); s.text(145, 178, "$120", 34, s.dark, bold=True)
    for k in range(4): y = 55 + k * 62; s.arrow(240, 155, 360, y + 20, s.light, 2); s.circ(400, y + 20, 22, s.main, s.dark, 3); s.text(400, y + 27, "P" + str(k + 1), 17, s.dark, bold=True); s.coin(480, y + 20, 18); s.text(520, y + 27, "$30", 18, s.light, "start", bold=True)
    s.rect(600, 110, 140, 90, s.dark, s.acc, 3); s.text(670, 148, "NACHA", 18, s.acc, bold=True); s.text(670, 175, "batch file", 14, s.light)
def s33(s):  # BNPL pay in 4: pies filling by quarters
    for k in range(4):
        x = 130 + k * 150; s.circ(x, 130, 48, s.dark, s.main, 5); a = (k + 1) * math.pi / 2
        if k == 3: s.circ(x, 130, 46, s.acc)
        else: s.path(f"M{x},130 L{x},82 A46,46 0 {1 if a > math.pi else 0} 1 {x + 46 * math.sin(a):.1f},{130 - 46 * math.cos(a):.1f} z", s.acc, s.dark, 2)
        s.text(x, 215, f"{k + 1}/4", 20, s.light, bold=True)
    s.text(400, 270, "Pay in 4 · APR (Reg Z)", 20, s.acc, bold=True); s.rect(330, 30, 140, 30, s.main, None, 0, 15); s.text(400, 51, "0% interest", 14, s.dark, bold=True)
def s34(s):  # insurance: umbrella + forms
    s.path("M80,150 a130,110 0 0 1 260,0 z", s.acc, s.dark, 4); s.line(210, 150, 210, 250, s.light, 6); s.path("M210,250 q0,18 -20,14", stroke=s.light, sw=6)
    for k in range(5): s.line(80 + k * 52, 150, 80 + (k + .5) * 52, 150, s.dark, 3)
    for k in range(3): s.line(130 + k * 20, 20 + k * 6, 120 + k * 20, 60 + k * 6, s.main, 3, op=.8)
    for k, n in enumerate(("Carrier A", "Carrier B", "Carrier C")): y = 60 + k * 70; s.rect(440, y, 260, 56, s.light, s.dark, 2, 6); s.text(520, y + 34, n, 17, s.dark, bold=True); s.text(660, y + 34, f"${90 + k * 14}", 18, s.main, bold=True)
    s.text(570, 290, "ACORD XML quotes", 14, s.light)
def s35(s):  # robo-advisor: growth + roundup
    s.line(80, 250, 80, 50, s.light, 2); s.line(80, 250, 460, 250, s.light, 2); s.path("M90,230 L160,200 L220,215 L290,150 L350,160 L440,70", stroke=s.acc, sw=6); s.poly([(440, 62), (452, 84), (426, 80)], s.acc)
    for k in range(5): s.rect(100 + k * 70, 250 - (20 + k * 18), 40, 20 + k * 18, s.main, None, 0, 3, op=.6)
    s.coin(560, 120, 30); s.text(560, 175, "$3.40 -> $4.00", 15, s.light); s.text(560, 200, "round-up .60", 15, s.acc); s.rect(500, 225, 120, 36, s.dark, s.main, 2); s.text(560, 249, "FIX 4.4", 16, s.main, bold=True)
def s36(s):  # budgeting: pie + cashflow
    cols = (s.acc, s.main, s.light, "#fb7185"); a0 = -math.pi / 2
    for k, f in enumerate((.35, .25, .22, .18)):
        a1 = a0 + f * 2 * math.pi; x0, y0 = 220 + 100 * math.cos(a0), 150 + 100 * math.sin(a0); x1, y1 = 220 + 100 * math.cos(a1), 150 + 100 * math.sin(a1)
        s.path(f"M220,150 L{x0:.1f},{y0:.1f} A100,100 0 {1 if f > .5 else 0} 1 {x1:.1f},{y1:.1f} z", cols[k], s.dark, 3); a0 = a1
    s.line(400, 150, 740, 150, s.light, 2, "4 6"); s.path("M410,200 L480,120 L550,180 L620,90 L690,170 L730,120", stroke=s.acc, sw=5); s.text(570, 250, "cash-flow gap", 20, s.light); s.text(570, 55, "OFX", 24, s.main, bold=True)
def s37(s):  # streaming: play + segments
    s.rect(70, 60, 330, 186, s.dark, s.light, 4, 12); s.poly([(190, 110), (190, 200), (280, 155)], s.acc, s.dark, 3)
    s.rect(70, 262, 330, 8, s.light, None, 0, 4, op=.4); s.rect(70, 262, 190, 8, s.acc, None, 0, 4)
    for k in range(5): s.rect(450 + k * 56, 80 + (k % 2) * 26, 48, 70, s.main if k < 3 else s.light, s.dark, 2, 4); s.text(474 + k * 56, 124 + (k % 2) * 26, f"s{k}", 16, s.dark, bold=True)
    s.text(590, 200, "Range: bytes=0-999", 15, s.acc); s.text(590, 230, "HLS .m3u8 · 240p 720p 1080p", 14, s.light)
def s38(s):  # ATM
    s.rect(110, 40, 250, 230, s.dark, s.main, 4, 12); s.rect(130, 60, 210, 80, "#06222f", s.light, 2, 4); s.text(235, 108, "ENTER PIN", 20, s.acc, bold=True)
    for r in range(4):
        for c in range(3): s.rect(150 + c * 62, 155 + r * 28, 52, 22, s.light, s.dark, 1.5, 3); s.text(176 + c * 62, 172 + r * 28, str(r * 3 + c + 1) if r < 3 else "*0#"[c], 14, s.dark, bold=True)
    s.rect(160, 255, 150, 12, "#02080c", s.light, 2, 2)
    for k in range(3): s.rect(420 + k * 30, 140 - k * 8, 130, 56, s.light, s.dark, 2, 4); s.text(485 + k * 30, 175 - k * 8, "$20", 24, s.dark, bold=True)
    s.arrow(370, 200, 410, 190, s.acc); s.text(580, 70, "ISO 8583", 22, s.acc, bold=True); s.text(580, 100, "ISO 9564 PIN block", 14, s.light)
def s39(s):  # POS terminal + EMV chip
    s.rect(120, 40, 190, 230, s.dark, s.main, 4, 14); s.rect(140, 60, 150, 60, "#08242a", s.light, 2, 4); s.text(215, 98, "$ 24.50", 22, s.acc, bold=True)
    for r in range(3):
        for c in range(3): s.rect(145 + c * 48, 135 + r * 30, 40, 24, s.light, s.dark, 1.5, 3)
    s.rect(380, 120, 190, 120, s.main, s.dark, 3, 12); s.rect(400, 148, 50, 40, s.acc, s.dark, 2, 5); s.line(400, 168, 450, 168, s.dark, 1.5); s.line(425, 148, 425, 188, s.dark, 1.5); s.text(500, 222, "EMV chip", 14, s.dark, bold=True); s.arrow(390, 108, 320, 130, s.light, 3); s.text(650, 150, "ARQC", 22, s.acc, bold=True); s.text(650, 182, "TC", 22, s.main, bold=True)
def s40(s):  # billing: invoice + calendar
    s.rect(80, 50, 220, 210, s.light, s.dark, 3, 6); s.text(190, 85, "INVOICE", 20, s.dark, bold=True)
    for k in range(4): s.line(100, 110 + k * 28, 200, 110 + k * 28, s.dark, 2, op=.6); s.text(280, 116 + k * 28, f"{10 + k * 5}.00", 14, s.dark, "end")
    s.text(250, 238, "$ 49.00", 22, s.main, "end", bold=True)
    s.rect(400, 60, 230, 190, s.dark, s.main, 3, 8); s.rect(400, 60, 230, 36, s.main, None, 0, 8); s.text(515, 86, "MONTHLY", 16, s.dark, bold=True)
    for r in range(4):
        for c in range(7): s.rect(410 + c * 30, 108 + r * 32, 24, 24, s.acc if (r, c) == (1, 3) else "#10223a", None, 0, 3)
    s.circ(690, 150, 40, s.acc, s.dark, 3); s.text(690, 160, "↻", 40, s.dark, bold=True)
def s41(s):  # flights
    s.path("M70,230 Q400,20 730,230", stroke=s.light, sw=3); 
    for t in range(0, 9): pass
    s.circ(70, 230, 14, s.acc); s.circ(730, 230, 14, s.acc); s.text(70, 262, "JFK", 18, s.light); s.text(730, 262, "LHR", 18, s.light)
    s.poly([(400, 62), (384, 100), (400, 92), (416, 100)], s.light, s.dark, 2); s.path("M370,86 L430,86", stroke=s.light, sw=8); s.line(400, 70, 400, 112, s.light, 8)
    for k, (c, p) in enumerate((("A", 412), ("B", 389), ("C", 455))): s.rect(210 + k * 140, 190 - k % 2 * 20, 110, 44, s.dark, s.main, 2, 6); s.text(265 + k * 140, 218 - k % 2 * 20, f"Carrier {c} ${p}", 14, s.light)
def s42(s):  # sports ticket + barcode
    s.rect(70, 70, 420, 150, s.acc, s.dark, 4, 12); s.line(380, 70, 380, 220, s.dark, 3, "8 8"); s.text(220, 118, "GAME DAY", 30, s.dark, bold=True); s.text(220, 158, "SEC 112 · ROW F · SEAT 7", 14, s.dark); s.circ(130, 190, 12, s.dark)
    for k in range(18): s.rect(396 + k * 5, 90, 2 + (k * 7) % 3, 110, s.dark)
    s.text(430, 214, "GS1 SGTIN", 11, s.dark, bold=True); s.circ(620, 130, 66, s.dark, s.main, 4); s.path("M590,130 a30,30 0 0 1 60,0 M620,100 v60 M590,130 h60", stroke=s.main, sw=3); s.text(620, 250, "rotating code 30 s", 14, s.light)
def s43(s):  # betting: odds board + chips
    s.rect(70, 50, 360, 200, s.dark, s.acc, 4, 8)
    for k, (t, o) in enumerate((("HOME", "+150"), ("DRAW", "+240"), ("AWAY", "-110"))): s.text(120, 100 + k * 60, t, 22, s.light, "start", bold=True); s.text(390, 100 + k * 60, o, 30, s.acc, "end", bold=True)
    for k, c in enumerate((s.acc, s.main, "#ef4444", s.light)): s.circ(540 + (k % 2) * 60, 220 - k * 22, 28, c, s.dark, 4); s.circ(540 + (k % 2) * 60, 220 - k * 22, 18, None, s.dark, 2, op=.7)
    s.text(620, 90, "back / lay", 18, s.light); s.text(620, 120, "decimal 2.50", 16, s.acc)
def s44(s):  # car rental
    s.path("M110,200 q10,-60 70,-62 l60,-34 h150 l70,36 q60,4 70,56 z", s.main, s.dark, 4); s.poly([(250, 112), (300, 112), (300, 140), (232, 140)], "#bfe7ff", s.dark, 2); s.poly([(310, 112), (360, 112), (390, 140), (310, 140)], "#bfe7ff", s.dark, 2)
    for x in (200, 440): s.circ(x, 205, 34, s.dark, s.light, 6); s.circ(x, 205, 10, s.light)
    s.circ(630, 110, 24, None, s.acc, 8); s.rect(646, 120, 80, 12, s.acc, None, 0, 3); s.rect(700, 132, 10, 20, s.acc); s.rect(684, 132, 10, 14, s.acc); s.text(620, 250, "OTA availability · reservation", 15, s.light)
def s45(s):  # IP + ping
    s.rect(70, 60, 330, 60, s.main, s.dark, 3); 
    for k, n in enumerate(("ver", "TTL", "proto", "src", "dst")): s.rect(70 + k * 66, 60, 66, 60, s.acc if k == 2 else s.main, s.dark, 2); s.text(103 + k * 66, 96, n, 14, s.dark, bold=True)
    s.rect(70, 120, 330, 80, s.dark, s.light, 3); s.text(235, 168, "ICMP echo  data", 18, s.light)
    s.circ(520, 150, 8, s.acc); 
    for k in range(4): s.circ(520, 150, 24 + k * 22, None, s.main, 3, op=.9 - k * .2)
    s.text(660, 120, "ping", 34, s.acc, bold=True); s.text(660, 160, "64 bytes", 16, s.light); s.text(660, 188, "ttl=64", 16, s.light)
def s46(s):  # IDT gates: doors
    for k in range(5):
        x = 90 + k * 135; s.rect(x, 80, 90, 150, s.dark, s.main, 4, 4); s.path(f"M{x},{80} a45,35 0 0 1 90,0", fill=s.dark, stroke=s.main, sw=4)
        s.rect(x + 8, 100, 74, 120, s.acc if k == 3 else "#102a44", s.light, 2, 2); s.circ(x + 70, 165, 5, s.light); s.text(x + 45, 262, f"INT {32 + k}", 14, s.light)
    s.text(400, 30, "install_gate(n, handler)", 20, s.acc, bold=True)
def sA(s):  # appendix A: assembly registers
    for k, r in enumerate(("RAX", "RBX", "RCX", "RDX")): s.rect(80 + k * 175, 60, 150, 50, s.dark, s.main, 3, 6); s.text(155 + k * 175, 92, r, 22, s.main, bold=True)
    s.text(80, 175, "mov rax, 5", 22, s.light, "start"); s.text(80, 210, "add rax, rbx", 22, s.light, "start"); s.text(80, 245, "jmp .loop", 22, s.light, "start"); s.text(430, 185, "0x48 0x89 0xC3", 24, s.acc, "start", bold=True); s.text(430, 225, "machine code", 16, s.main, "start")
def sB(s):  # appendix B: terminal
    s.rect(80, 40, 640, 220, "#050b12", s.light, 4, 10); s.rect(80, 40, 640, 30, s.main, None, 0, 10)
    for k, c in enumerate(("#ef4444", "#facc15", "#22c55e")): s.circ(104 + k * 24, 55, 6, c)
    for k, (t, c) in enumerate((("$ ls -l | grep .c", s.light), ("$ cat kernel.c | wc -l", s.light), ("$ make && qemu-system-x86_64", s.acc), ("$ _", s.main))): s.text(104, 108 + k * 40, t, 20, c, "start", bold=k == 2)
def sC(s):  # appendix C: C code
    s.text(130, 160, "{", 190, s.main, bold=True, op=.9); s.text(670, 160, "}", 190, s.main, bold=True, op=.9)
    for k, t in enumerate(("int main(void) {", "  char *p = kmalloc(64);", "  p[0] = 'A';", "  return 0;", "}")): s.text(230, 80 + k * 38, t, 20, s.light if k not in (1, 2) else s.acc, "start")
def sD(s):  # appendix D: answers
    for k in range(3): y = 60 + k * 68; s.circ(130, y + 22, 24, s.acc, s.dark, 3); s.path(f"M{118},{y + 22} l9,10 l16,-20", stroke=s.dark, sw=6); s.rect(180, y, 440, 44, s.dark, s.main, 2, 6); s.line(196, y + 22, 560 - k * 50, y + 22, s.light, 3, op=.6)
    s.text(400, 275, "Q  →  A", 20, s.main, bold=True)
def hero(s):  # landing page: the whole stack, from silicon up to the applied chapters
    s.rect(60, 232, 680, 42, s.dark, s.main, 3, 6); s.text(400, 260, "hardware: CPU · RAM · disk · network card", 16, s.main)
    s.chip(90, 162, 56, "CPU"); s.rect(166, 168, 60, 44, "#0e3b2e", s.light, 2); s.text(196, 195, "RAM", 13, s.light); s.circ(262, 190, 24, "#1b1f27", s.light, 3); s.circ(262, 190, 5, s.light); s.rect(306, 170, 66, 40, "#0e3b2e", s.acc, 2); s.text(339, 195, "NIC", 13, s.acc, bold=True)
    s.rect(60, 112, 330, 42, s.main, s.dark, 3, 6); s.text(225, 139, "KERNEL: paging · tasks · FAT16 · TCP/IP", 12, s.dark, bold=True)
    s.rect(60, 58, 330, 40, s.acc, s.dark, 3, 6); s.text(225, 84, "boot: GRUB -> _start -> kmain", 13, s.dark, bold=True)
    s.bank(470, 40, .62); s.rect(580, 52, 64, 100, s.dark, s.light, 2, 6); s.rect(588, 62, 48, 26, "#06222f", s.main, 1.5, 2); s.text(612, 80, "ATM", 11, s.acc, bold=True)
    for r in range(3):
        for c in range(3): s.rect(590 + c * 15, 96 + r * 14, 12, 10, s.light, None, 0, 2)
    s.path("M672,70 a42,34 0 0 1 84,0 z", s.acc, s.dark, 2); s.line(714, 70, 714, 112, s.light, 3)
    s.poly([(500, 190), (480, 205), (500, 200), (520, 205)], s.light, s.dark, 2); s.line(500, 176, 500, 214, s.light, 6); s.line(482, 192, 518, 192, s.light, 6)
    s.packet(540, 176, 100, 34, "IP", s.main); s.coin(690, 195, 20); s.arrow(400, 135, 460, 135, s.light, 3); s.arrow(400, 200, 470, 200, s.light, 2)
    s.text(400, 30, "Unix OS from Scratch", 22, s.light, bold=True)

def s47(s):  # eBay-style auction: the lamp, a gavel, rising bids, SOLD
    # the lamp on a pedestal
    s.rect(70, 235, 120, 22, s.light, s.dark, 2, 4); s.rect(90, 257, 80, 14, s.main, s.dark, 2, 3)
    s.path("M130,235 v-70", stroke=s.light, sw=6); s.poly([(95, 165), (165, 165), (150, 110), (110, 110)], s.acc, s.dark, 3)
    s.circ(130, 175, 34, "#fff7c2", None, 0, op=.25); s.text(130, 90, "LOT 110001", 13, s.light, bold=True)
    # the gavel, striking its block
    s.rect(240, 232, 130, 24, s.main, s.dark, 3, 4)
    s.add('<g transform="rotate(-32 330 170)"><rect x="262" y="160" width="96" height="16" rx="5" fill="#a8672d" stroke="#170c04" stroke-width="3"/><rect x="306" y="120" width="60" height="42" rx="7" fill="#c98a45" stroke="#170c04" stroke-width="3"/></g>')
    for k in range(3): s.path(f"M{262 - k * 12},{205 - k * 8} q-8,8 0,16", stroke=s.acc, sw=3, op=.8 - k * .25)
    # the ladder of bids, each a paddle with its price
    for k, (t, w) in enumerate((("$9.99", 70), ("$15.50", 110), ("$40.00", 170), ("$45.00", 230))):
        y = 64 + k * 46; s.rect(430, y, w + 40, 34, s.main if k < 3 else s.acc, s.dark, 2, 6, op=.95); s.text(448, y + 23, t, 18, s.dark, "start", bold=True)
        s.text(430 + w + 56, y + 23, "user %d" % (4, 5, 6, 6)[k], 13, s.light, "start", op=.85)
    s.add('<g transform="rotate(-8 693 272)"><rect x="620" y="250" width="146" height="42" rx="6" fill="none" stroke="#f87171" stroke-width="5"/><text x="693" y="281" font-family="ui-monospace,Menlo,Consolas,monospace" font-size="26" font-weight="bold" fill="#f87171" text-anchor="middle">SOLD</text></g>')
