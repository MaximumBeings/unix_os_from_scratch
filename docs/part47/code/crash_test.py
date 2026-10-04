#!/usr/bin/env python3
"""Chapter 47: REAL hard kills. The kernel is booted in QEMU on a fresh disk image and QEMU is killed with SIGKILL (the process dies at once; nothing is flushed or shut down by the guest -- the
nearest thing to pulling the plug that an emulator offers) at a pseudo-random moment while the marketplace demo is writing its write-ahead log. Whatever is on the disk image at that instant is
then read by the independent Python reader (mkt_model.py): the FAT16 volume is parsed, the log records are checked (magic, length, sequence number, CRC-32), the valid PREFIX is replayed through the
independent Python marketplace, and the marketplace invariants are asserted. Expected, if the design is right: every kill leaves a log whose valid prefix replays to a consistent marketplace,
whatever happens to the record that was being written. NOTE: the demo's own Part 5 deliberately damages records; kills landing there test the same reader on deliberately damaged disks, and are
reported separately. Usage: crash_test.py OUTDIR N [SEED]    (each boot takes about 15 s; N=20 takes about 6 minutes)"""
import os, random, re, signal, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkt_model import read_volume, decode_log, Market
out, N = sys.argv[1], int(sys.argv[2]); seed = int(sys.argv[3]) if len(sys.argv) > 3 else 47
P1 = "Part 1: funding three buyers"   # specific to this chapter: earlier chapters also print "Part 1:"
def boot_until(marker, serial, timeout=120):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            if marker in open(serial, errors="replace").read(): return time.time() - t0
        except FileNotFoundError: pass
        time.sleep(0.001)
    return None
def start(tag):
    disk = os.path.join(out, f"crash_{tag}.img"); serial = os.path.join(out, f"crash_{tag}.txt")
    for f in (disk, serial):
        if os.path.exists(f): os.remove(f)
    subprocess.run(["qemu-img", "create", "-q", "-f", "raw", disk, "8M"], check=True)
    p = subprocess.Popen(["qemu-system-x86_64", "-cdrom", os.path.join(out, "os.iso"), "-drive", f"file={disk},format=raw,if=ide,index=0", "-serial", f"file:{serial}", "-display", "none",
                          "-no-reboot", "-m", "64M", "-netdev", "user,id=n0", "-device", "rtl8139,netdev=n0"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return p, disk, serial
if N == 0:   # calibration: when does each phase begin, in seconds after "Part 1" first appears on the serial port? (polled every millisecond)
    p, disk, serial = start("cal"); boot_until(P1, serial); t0 = time.time(); marks = {}
    pending = ["Part 2: seller", "Part 3: the bidding war", "Part 4: the auction ends", "Part 5: crash recovery", "Bit rot:", "Chapter 47 marketplace demo complete"]
    while pending and time.time() - t0 < 60:
        txt = open(serial, errors="replace").read()
        for m in list(pending):
            if m in txt: marks[m] = time.time() - t0; pending.remove(m)
        time.sleep(0.001)
    p.kill(); print({k: round(v, 3) for k, v in marks.items()}); sys.exit(0)
rng = random.Random(seed); rows = []; bad = 0
for i in range(N):
    p, disk, serial = start(f"{i}")
    if boot_until(P1, serial) is None: p.kill(); print("boot never reached the demo"); sys.exit(1)
    delay = rng.uniform(0.0, float(os.environ.get("MAXDELAY", "1.2"))); time.sleep(delay); p.send_signal(signal.SIGKILL); p.wait()
    ser = open(serial, errors="replace").read(); seg = ser[ser.find("Starting this chapter's own marketplace demo"):]
    part = max([int(m) for m in re.findall(r"^Part (\d): (?:funding|seller|the bidding|the auction|crash)", seg, re.M)] + [0])
    geo, files = read_volume(disk); cmds, why, nfiles = decode_log(files)
    mk = Market(); ok = True
    try:
        for c in cmds: mk.apply(c)
    except AssertionError as e: ok = False; why += " / INVARIANT: " + str(e)
    bad += not ok
    rows.append((i, delay, part, nfiles, len(cmds), why, ok))
    print(f"kill {i:2d}: SIGKILL {delay:5.3f} s after Part 1 began (demo was in Part {part}); {nfiles:2d} log files on the disk, {len(cmds):2d} valid records replayed; decoding stopped: {why}; invariants {'hold' if ok else 'BROKEN'}", flush=True)
inpart = {}
for r in rows: inpart.setdefault(r[2], []).append(r)
print(f"\n{N} hard kills: {N - bad} left a log whose valid prefix replays to a consistent marketplace, {bad} did not. Kills by demo phase (Part number when the kill landed): " + ", ".join(f"Part {k}: {len(v)}" for k, v in sorted(inpart.items())))
torn = [r for r in rows if r[5] != "log ends"]
print(f"{len(torn)} of the kills left a damaged or inconsistent log tail that the reader had to REJECT instead of replaying" + ((": " + "; ".join(f"kill {r[0]} ({r[5]})" for r in torn[:6])) if torn else " (every kill landed between disk operations: no torn record was observed; the torn-tail path is exercised by the host tests and by the kernel's own injected tear, not by these kills)"))
sys.exit(1 if bad else 0)
