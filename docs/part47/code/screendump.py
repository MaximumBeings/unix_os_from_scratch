#!/usr/bin/env python3
"""Chapter 47: boot OUTDIR/os.iso, wait until the marketplace demo prints its completion marker, then ask QEMU for a screendump of the VGA console (through QEMU's monitor socket) and write it as a PNG
(pure Python: PPM -> PNG with zlib, no imaging library). Usage: screendump.py OUTDIR OUT.png"""
import os, socket, struct, subprocess, sys, time, zlib
out, png = sys.argv[1], sys.argv[2]; serial = os.path.join(out, "sd_serial.txt"); disk = os.path.join(out, "sd_disk.img"); sock = "/tmp/c47_monitor.sock"; ppm = os.path.join(out, "sd.ppm")   # (a short socket path: AF_UNIX paths are limited to about 100 bytes)
for f in (serial, disk, sock, ppm):
    if os.path.exists(f): os.remove(f)
subprocess.run(["qemu-img", "create", "-q", "-f", "raw", disk, "8M"], check=True)
p = subprocess.Popen(["qemu-system-x86_64", "-cdrom", os.path.join(out, "os.iso"), "-drive", f"file={disk},format=raw,if=ide,index=0", "-serial", f"file:{serial}", "-display", "none", "-no-reboot", "-m", "64M",
                      "-netdev", "user,id=n0", "-device", "rtl8139,netdev=n0", "-monitor", f"unix:{sock},server,nowait"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()
while time.time() - t0 < 150:
    try:
        if "Chapter 47 marketplace demo complete" in open(serial, errors="replace").read(): break
    except FileNotFoundError: pass
    time.sleep(0.2)
time.sleep(1.0)
s = socket.socket(socket.AF_UNIX); s.connect(sock); s.recv(4096); s.sendall(f"screendump {ppm}\n".encode()); time.sleep(1.0); s.close(); p.kill()
d = open(ppm, "rb").read(); assert d[:2] == b"P6"; parts = d.split(None, 4); w, h = int(parts[1]), int(parts[2]); pix = d[len(d) - w * h * 3:]
raw = b"".join(b"\0" + pix[y * w * 3:(y + 1) * w * 3] for y in range(h))
def chunk(t, b): return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)
open(png, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
print(f"wrote {png}: {w}x{h} pixels")
