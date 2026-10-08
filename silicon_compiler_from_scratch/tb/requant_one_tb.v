// Chapter 3, running example A: run the requantizer on a few hand-chosen cases given on the command line (+acc=, +m=, +s=, +relu=) and print the result. Combinational circuit, so one #1 is enough.
`timescale 1ns/1ps
module requant_one_tb;
    reg signed [31:0] acc; reg [23:0] m; reg [5:0] s; reg relu; wire signed [7:0] q; integer a, mi, si, ri;
    requant dut (.acc(acc), .m(m), .s(s), .relu(relu), .q(q));
    initial begin
        if (!$value$plusargs("acc=%d", a) || !$value$plusargs("m=%d", mi) || !$value$plusargs("s=%d", si) || !$value$plusargs("relu=%d", ri)) begin $display("usage: +acc= +m= +s= +relu="); $finish; end
        acc = a; m = mi; s = si; relu = ri; #1; $display("Q %0d", q); $finish;
    end
endmodule
