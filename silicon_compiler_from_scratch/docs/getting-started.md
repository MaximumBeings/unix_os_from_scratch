# Getting Started

Everything in this book runs on **Python 3 and three open-source programs**. There is nothing to buy and nothing to license.

## What you need

| Program | What it does in this book | Version used for every number in the book |
|---|---|---|
| **Icarus Verilog** (`iverilog`, `vvp`) | The reference simulator; every testbench runs here | 12.0 |
| **Verilator** | A second, independent simulator (it compiles the design to C++); every testbench also runs here | 5.020 |
| **Yosys** | Synthesis: turns the Verilog into gates and counts them | 0.33 |
| **Python 3** | Golden models, the assembler, the compiler, and the test scripts | 3.11 |
| MkDocs + Material (optional) | To build this site yourself | any recent version |

## Installing

**Debian and Ubuntu (and Windows under WSL2):**

```bash
sudo apt-get update
sudo apt-get install -y iverilog verilator yosys python3 make g++
```

**Fedora:** `sudo dnf install iverilog verilator yosys python3 make gcc-c++`

**macOS (Homebrew):** `brew install icarus-verilog verilator yosys python3`

**Arch:** `sudo pacman -S iverilog verilator yosys python make gcc`

Newer or older versions of the tools will usually work, but a synthesis number can change by a few cells between Yosys versions, and the book's numbers are the ones its versions produced.

## Check that it works

From the book's root directory:

```bash
iverilog -V | head -1      # Icarus Verilog version ...
verilator --version        # Verilator 5....
yosys -V                   # Yosys 0....
python3 tools/ch01_flow.py # Chapter 1's whole flow: should end with a Yosys cell count
```

If the last command prints `PASS: all 512 cases match the golden model` twice (once per simulator) and two synthesis summaries, everything is installed correctly.

## How the repository is laid out

```text
rtl/     the Verilog (one file per module, kept stable once a chapter has used it)
tb/      self-checking testbenches
model/   the Python golden models, the assembler, the compiler
tools/   hw.py (the toolbox around the three programs) and one script per chapter
out/     test vectors and the recorded outputs the pages show
docs/    the chapters
```

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `iverilog: command not found` | The package name differs by system: `iverilog` on Linux, `icarus-verilog` on Homebrew. |
| `syntax error` on a file that looks fine | A SystemVerilog keyword used as a name (`bit`, `logic`, `int`, ...) under `-g2012`: rename it. |
| Verilator prints hundreds of warnings | The book runs it with `-Wno-fatal -Wno-lint -Wno-style`; the warnings are lint, not errors. |
| Yosys numbers differ slightly from the book | A different Yosys version. The book's figures are for 0.33. |
| A testbench cannot find `out/...hex` | Run scripts from the book's root directory: the testbenches read their vectors with paths relative to it. |
