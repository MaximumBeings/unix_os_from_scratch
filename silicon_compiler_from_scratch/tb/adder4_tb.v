// Chapter 1: a self-checking testbench. It applies all 512 inputs and compares the circuit's output with the answers the Python golden model wrote.
// It prints one line, PASS or FAIL, and calls $fatal on a mismatch so that the exit status of the simulator says what happened.
`timescale 1ns/1ps
module adder4_tb;
    reg [3:0] a, b; reg cin; wire [3:0] sum; wire cout;
    adder4 dut(.a(a), .b(b), .cin(cin), .sum(sum), .cout(cout));
    reg [13:0] vec [0:511]; integer i, bad; reg [13:0] v;
    initial begin
        $readmemh("out/adder4_vectors.hex", vec); bad = 0;
        for (i = 0; i < 512; i = i + 1) begin
            v = vec[i]; a = v[13:10]; b = v[9:6]; cin = v[5]; #1;
            if ({sum, cout} !== {v[4:1], v[0]}) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH a=%0d b=%0d cin=%0d: got sum=%0d cout=%0d, want sum=%0d cout=%0d", a, b, cin, sum, cout, v[4:1], v[0]); end
        end
        if (bad == 0) $display("PASS: all 512 cases match the golden model"); else begin $display("FAIL: %0d of 512 cases differ", bad); $fatal(1, "adder4 failed"); end
        $finish;
    end
endmodule
