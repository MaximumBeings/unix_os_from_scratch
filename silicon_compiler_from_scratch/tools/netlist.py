#!/usr/bin/env python3
"""Chapter 10: netlist utilities. Read the JSON netlist that Yosys writes after mapping to the toy library and write it back out as
  to_verilog(json, top, fault=None)  a Verilog netlist of toy cells (simulate it with lib/toy_cells.v),
  to_blif(json, top, fault=None, tie=None)  a BLIF file (for ABC's combinational equivalence checker); `tie` holds named inputs (e.g. {'m_23': 1}) at constants,
optionally with ONE STUCK-AT FAULT injected: fault = (net, value) forces that net (the output of one gate, identified by its bit number in the JSON) to the constant `value` (0 or 1).
  nets(json, top)                    the list of gate-output nets that can carry a fault."""
import json, re, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FUNC = {"BUF": lambda a: a[0], "INV": lambda a: 1 - a[0], "NAND2": lambda a: 1 - (a[0] & a[1]), "NOR2": lambda a: 1 - (a[0] | a[1]), "AND2": lambda a: a[0] & a[1], "OR2": lambda a: a[0] | a[1],
        "XOR2": lambda a: a[0] ^ a[1], "XNOR2": lambda a: 1 - (a[0] ^ a[1]), "NAND3": lambda a: 1 - (a[0] & a[1] & a[2]), "NOR3": lambda a: 1 - (a[0] | a[1] | a[2]),
        "AOI21": lambda a: 1 - ((a[0] & a[1]) | a[2]), "OAI21": lambda a: 1 - ((a[0] | a[1]) & a[2]), "MUX2": lambda a: a[1] if a[2] else a[0]}
PINS = {"BUF": ["A"], "INV": ["A"], "NAND2": ["A", "B"], "NOR2": ["A", "B"], "AND2": ["A", "B"], "OR2": ["A", "B"], "XOR2": ["A", "B"], "XNOR2": ["A", "B"], "NAND3": ["A", "B", "C"], "NOR3": ["A", "B", "C"],
        "AOI21": ["A", "B", "C"], "OAI21": ["A", "B", "C"], "MUX2": ["A", "B", "S"]}
def load(path, top): return json.load(open(path))["modules"][top]
def nets(path, top): return [c["connections"]["Y"][0] for c in load(path, top)["cells"].values() if c["type"] in FUNC]
def _n(b): return f"n{b}" if isinstance(b, int) else {"0": "1'b0", "1": "1'b1", "x": "1'bx"}[b]
def to_verilog(path, top, fault=None):
    m = load(path, top); L = []; names = [f"{p}" for p in m["ports"]]
    L.append(f"module {top}({', '.join(names)});")
    for p, d in m["ports"].items(): L.append(f"  {d['direction']} [{len(d['bits'])-1}:0] {p};")
    bits = set()
    for c in m["cells"].values():
        for pn, bs in c["connections"].items(): bits.update(b for b in bs if isinstance(b, int))
    for p, d in m["ports"].items(): bits.update(b for b in d["bits"] if isinstance(b, int))
    for b in sorted(bits): L.append(f"  wire n{b};")
    for p, d in m["ports"].items():
        for i, b in enumerate(d["bits"]):
            if d["direction"] == "input": L.append(f"  assign {_n(b)} = {p}[{i}];")
            else: L.append(f"  assign {p}[{i}] = {_n(b)};")
    for name, c in m["cells"].items():
        t = c["type"]; conn = c["connections"]; q = re.sub(r"[^\w]", "_", name)
        if fault and t in FUNC and conn["Y"][0] == fault[0]:
            L.append(f"  {t} g_{q} ({', '.join('.' + pn + '(' + _n(conn[pn][0]) + ')' for pn in PINS[t])}, .Y());"); L.append(f"  assign {_n(fault[0])} = 1'b{fault[1]};"); continue
        if t in FUNC: L.append(f"  {t} g_{q} ({', '.join('.' + pn + '(' + _n(conn[pn][0]) + ')' for pn in PINS[t])}, .Y({_n(conn['Y'][0])}));")
        elif t == "DFF": L.append(f"  DFF g_{q} (.D({_n(conn['D'][0])}), .CK({_n(conn['CK'][0])}), .Q({_n(conn['Q'][0])}));")
        else:
            ports = ", ".join(f".{pn}({{{', '.join(_n(b) for b in reversed(bs))}}})" for pn, bs in conn.items()); L.append(f"  {t} {q} ({ports});")
    L.append("endmodule"); return "\n".join(L) + "\n"
def to_blif(path, top, fault=None, tie=None):
    m = load(path, top); L = [f".model {top}"]; ins = []; outs = []
    for p, d in m["ports"].items():
        for i, b in enumerate(d["bits"]): (ins if d["direction"] == "input" else outs).append((f"{p}_{i}", b))
    L.append(".inputs " + " ".join(n for n, _ in ins)); L.append(".outputs " + " ".join(n for n, _ in outs))
    tie = tie or {}
    for n, b in ins: L.append(f".names {n} {_n(b)}\n1 1" if n not in tie else f".names {_n(b)}\n" + ("1" if tie[n] else ""))     # tie: an input held at a constant (an operating constraint)
    L.append(".names const0\n"); L.append(".names const1\n1")
    def sig(b): return _n(b) if isinstance(b, int) else {"0": "const0", "1": "const1", "x": "const0"}[b]
    for name, c in m["cells"].items():
        t = c["type"]; conn = c["connections"]
        if t not in FUNC: raise ValueError("BLIF export supports combinational designs only: " + t)
        y = conn["Y"][0]
        if fault and y == fault[0]: L.append(f".names {_n(y)}\n" + ("1" if fault[1] else "")); continue
        pins = [sig(conn[pn][0]) for pn in PINS[t]]; k = len(pins); L.append(".names " + " ".join(pins) + f" {_n(y)}")
        for v in range(1 << k):
            a = [(v >> (k - 1 - i)) & 1 for i in range(k)]
            if FUNC[t](a): L.append("".join(map(str, a)) + " 1")
    for n, b in outs: L.append(f".names {sig(b)} {n}\n1 1")
    L.append(".end"); return "\n".join(L) + "\n"
def to_verilog_faultable(path, top, sites, modname):
    """Like to_verilog, but with an injection point on every net in `sites`: extra ports `fsel` (16 bits) and `fval`; when fsel == k the k-th site is forced to fval. fsel = 16'hFFFF means no fault.
    One compiled netlist therefore serves every fault (the testbench just changes fsel)."""
    m = load(path, top); idx = {n: i for i, n in enumerate(sites)}; L = []
    L.append(f"module {modname}({', '.join(list(m['ports']) + ['fsel', 'fval'])});"); L.append("  input [15:0] fsel; input fval;")
    for p, d in m["ports"].items(): L.append(f"  {d['direction']} [{len(d['bits'])-1}:0] {p};")
    bits = set()
    for c in m["cells"].values():
        for pn, bs in c["connections"].items(): bits.update(b for b in bs if isinstance(b, int))
    for p, d in m["ports"].items(): bits.update(b for b in d["bits"] if isinstance(b, int))
    for b in sorted(bits): L.append(f"  wire n{b};")
    for b in sites: L.append(f"  wire g{b}; assign n{b} = (fsel == 16'd{idx[b]}) ? fval : g{b};")
    for p, d in m["ports"].items():
        for i, b in enumerate(d["bits"]):
            if d["direction"] == "input": L.append(f"  assign {_n(b)} = {p}[{i}];")
            else: L.append(f"  assign {p}[{i}] = {_n(b)};")
    for name, c in m["cells"].items():
        t = c["type"]; conn = c["connections"]; q = re.sub(r"[^\w]", "_", name)
        if t not in FUNC: raise ValueError("combinational designs only")
        y = conn["Y"][0]; yn = f"g{y}" if y in idx else _n(y)
        L.append(f"  {t} g_{q} ({', '.join('.' + pn + '(' + _n(conn[pn][0]) + ')' for pn in PINS[t])}, .Y({yn}));")
    L.append("endmodule"); return "\n".join(L) + "\n"
