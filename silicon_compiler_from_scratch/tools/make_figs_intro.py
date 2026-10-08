#!/usr/bin/env python3
"""Figures for the Introduction page -> docs/assets/fig/intro-*.svg"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); from figs import Fig, C, bar_chart
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "assets", "fig")
# the stack
f = Fig(940, 470, "The stack the book builds"); f.text(470, 24, "The book builds a whole stack, from a model description down to gates, and tests every layer", 14, bold=True)
L = [("A model (a graph of tensor operations)", "what the user writes; floating point", C["gray2"], C["gray"], "Ch 11, 12"), ("Capra, the compiler", "scales, tiles, scratchpad addresses, instructions", C["orange2"], C["orange"], "Ch 11"), ("GA-2 instructions (128-bit)", "LD ST MM RQ SM VADD AMAX HALT (+ UNPACK)", C["yellow2"], C["gray"], "Ch 7"),
     ("The chip's units: array, requantizer, softmax, DMA", "systolic MACs, int8 arithmetic, exp table, divider, scratchpad", C["blue2"], C["blue"], "Ch 2-6"), ("Verilog (the RTL)", "tested in two simulators against Python golden models", C["green2"], C["green"], "Ch 1-9"), ("Gates (a netlist of library cells)", "synthesized, timed, gate-level simulated, fault graded", C["red2"], C["red"], "Ch 10")]
for k, (a, b, fl, st, ch) in enumerate(L):
    y = 54 + k * 62; f.box(120, y, 640, 52, [a], fl, st, 14, True, sub=b); f.text(785, y + 31, ch, 12.5, st, "start", True)
    if k < 5: f.arrow(440, y + 54, 440, y + 62, C["ink"], 2)
f.text(60, 240, "up: what you want", 11.5, C["line"], rot=-90, italic=True); f.arrow(36, 400, 36, 60, C["gray"], 1.8, 9, "5 4")
f.text(470, 440, "Case studies (Part 6) then add modern techniques to the same stack: int4 weights, grouped-query attention, paged KV cache, speculative decoding, mixture of experts", 11.5, C["line"], "middle", False, False, True)
f.text(470, 458, "Every layer has its own test, and every test is itself tested by breaking the layer on purpose.", 12, C["red"], "middle", True); f.save(f"{OUT}/intro-stack.svg")
# roadmap
f = Fig(940, 400, "Roadmap"); f.text(470, 24, "The six parts: each ends with something that runs", 14, bold=True)
P = [("1", "Foundations", "Ch 1-3", "the test loop; MAC; int8 and the requantizer", C["blue2"], C["blue"]), ("2", "The Compute Engine", "Ch 4-6", "systolic array; scratchpad + DMA; softmax", C["green2"], C["green"]), ("3", "The Whole Chip", "Ch 7-9", "ISA + sequencer; attention + KV cache; batching, roofline", C["orange2"], C["orange"]), ("4", "Verilog to Gates", "Ch 10", "synthesis, timing, gate-level verification, fault grading", C["red2"], C["red"]), ("5", "Compilation", "Ch 11-12", "Capra; a tiny transformer decodes on the chip", C["purple2"], C["purple"]), ("6", "Case Studies", "Ch 13-17", "int4, GQA, paged KV, speculative decoding, MoE", C["teal2"], C["teal"])]
for k, (n, t, ch, d, fl, st) in enumerate(P):
    x = 20 + (k % 3) * 306; y = 56 + (k // 3) * 170; f.box(x, y, 292, 142, [], fl, st, sw=2); f.text(x + 18, y + 38, n, 30, st, "start", True); f.text(x + 52, y + 32, t, 15, C["ink"], "start", True); f.text(x + 52, y + 54, ch, 12, st, "start", True)
    import textwrap; f.lines(x + 16, y + 88, textwrap.wrap(d, 40), 12, C["line"], "start", lh=18)
f.text(470, 392, "plus: Getting Started, Background (Verilog and the tools), the Appendices (primers) and the answers to every chapter's questions", 12, C["line"], italic=True); f.save(f"{OUT}/intro-roadmap.svg")
# a token's journey
f = Fig(940, 330, "A token's journey"); f.text(470, 24, "One generated token touches every chapter: the book in a single decode step", 14, bold=True)
S = [("host: embedding row", "Ch 12", C["orange2"], C["orange"]), ("LD: DMA brings weights in", "Ch 5", C["green2"], C["green"]), ("MM: int8 matrix products", "Ch 2-4", C["blue2"], C["blue"]), ("RQ: back to int8", "Ch 3", C["blue2"], C["blue"]), ("append k, v to the KV cache", "Ch 8", C["purple2"], C["purple"]), ("SM: softmax", "Ch 6", C["teal2"], C["teal"]), ("MM + RQ: sum, feed-forward", "Ch 3, 4, 8", C["blue2"], C["blue"]), ("AMAX: pick the next token", "Ch 7", C["yellow2"], C["gray"])]
for k, (t, ch, fl, st) in enumerate(S):
    x = 14 + (k % 4) * 228; y = 56 + (k // 4) * 100; f.box(x, y, 212, 64, [t], fl, st, 12, True, sub=ch)
    if k % 4 < 3: f.arrow(x + 214, y + 32, x + 226, y + 32, C["ink"], 2)
f.lines(470, 250, ["The steps above are the instructions of a program written by Capra (Ch 11), run by the sequencer (Ch 7)", "on Verilog (Ch 1-6) that synthesizes to gates (Ch 10), at a speed set by memory traffic (Ch 9)."], 12.5, C["ink"], lh=20)
f.text(470, 306, "The output token is fed back as the next input, and the loop repeats", 12, C["line"], italic=True); f.save(f"{OUT}/intro-token.svg")
# evidence: broken things caught per chapter
ch = bar_chart(900, 360, ["1", "2", "3", "4", "5", "6", "7", "8", "9", "11"], [[10, 19, 15, 16, 20, 27, 49, 23, 16, 27]], "Deliberately broken versions caught by the tests, per chapter (every real mutant; equivalent ones excluded)", "mutants caught", colors=[C["red"]], fmt="{:.0f}", maxv=56); ch.save(f"{OUT}/intro-evidence.svg")
print("4 figures written")
