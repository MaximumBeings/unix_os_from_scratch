#!/usr/bin/env python3
"""Chapter 54: extracts the "Simple_1RTT_Handshake" section of the Botan test data (data/tls/botan_rfc8448_transcripts.vec, BSD licence; the handshake of RFC 8448 section 3) into data/tls/rfc8448_simple.txt, the small file the kernel embeds
with incbin. Each line is "Name = hex". Only the fields the kernel replays are kept."""
import os
here = os.path.dirname(os.path.abspath(__file__)); keep = ["Client_RNG_Pool", "Record_ClientHello_1", "Record_ServerHello", "Record_ServerHandshakeMessages", "Record_ClientFinished", "Record_NewSessionTicket", "Client_AppData", "Record_Client_AppData", "Server_AppData", "Record_Server_AppData", "Record_Client_CloseNotify", "Record_Server_CloseNotify"]
sec = None; out = []
for l in open(os.path.join(here, "data/tls/botan_rfc8448_transcripts.vec")):
    l = l.strip()
    if l.startswith("["): sec = l; continue
    if sec == "[Simple_1RTT_Handshake]" and " = " in l and not l.startswith("#"):
        k, v = l.split(" = ", 1)
        if k in keep: out.append(f"{k} = {v}")
open(os.path.join(here, "data/tls/rfc8448_simple.txt"), "w").write("\n".join(out) + "\n"); print(len(out), "fields")
