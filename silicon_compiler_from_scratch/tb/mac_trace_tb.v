// Chapter 2, running example A: drive the MAC with a short hand-chosen program and print one line per clock cycle (inputs, product, accumulator after the edge). tools/ch02_example_a.py compares these lines with a hand calculation written independently in Python.
`timescale 1ns/1ps
module mac_trace_tb;
    reg clk = 0, rst = 1, clr = 0, en = 0; reg signed [7:0] a = 0, b = 0; wire signed [31:0] acc;
    mac dut (.clk(clk), .rst(rst), .clr(clr), .en(en), .a(a), .b(b), .acc(acc));
    always #5 clk = ~clk;
    task cyc(input c, input e, input signed [7:0] x, input signed [7:0] y);
        begin @(negedge clk); clr = c; en = e; a = x; b = y; @(posedge clk); #1; $display("T %0d %0d %0d %0d %0d", c, e, x, y, acc); end
    endtask
    initial begin
        @(posedge clk); #1; rst = 0;
        cyc(1, 1, 3, 4);        // start a sum with 3*4
        cyc(0, 1, -5, 6);       // + (-5)*6
        cyc(0, 1, 100, -2);     // + 100*(-2)
        cyc(0, 0, 99, 99);      // en = 0: the inputs are ignored, the accumulator holds
        cyc(0, 1, -128, -128);  // + (-128)*(-128) = +16384
        cyc(0, 1, 7, -9);
        cyc(0, 1, -1, 127);
        cyc(1, 1, 2, 2);        // clr with en: a new sum begins with 2*2 (the old sum is dropped)
        cyc(0, 1, 10, 10);
        cyc(1, 0, 55, 55);      // clr without en: the accumulator becomes 0
        $finish;
    end
endmodule
