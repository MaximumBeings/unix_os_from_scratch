// Chapter 1: a 4-bit ripple-carry adder built from four full adders. The smallest circuit that still has all the parts of the flow:
// combinational logic, a hierarchy, a testbench, a golden model written in another language, and numbers from synthesis.
module full_adder(input a, input b, input cin, output sum, output cout);
    assign sum  = a ^ b ^ cin;
    assign cout = (a & b) | (cin & (a ^ b));
endmodule

module adder4(input [3:0] a, input [3:0] b, input cin, output [3:0] sum, output cout);
    wire [4:0] c;
    assign c[0] = cin;
    genvar i;
    generate for (i = 0; i < 4; i = i + 1) begin : stage
        full_adder fa(.a(a[i]), .b(b[i]), .cin(c[i]), .sum(sum[i]), .cout(c[i + 1]));
    end endgenerate
    assign cout = c[4];
endmodule
