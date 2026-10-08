// Simulation models of the toy library (generated together with lib/toy.lib by tools/gen_toylib.py).
module BUF(input A, output Y); assign Y = A; endmodule
module INV(input A, output Y); assign Y = ~A; endmodule
module NAND2(input A, input B, output Y); assign Y = ~(A & B); endmodule
module NOR2(input A, input B, output Y); assign Y = ~(A | B); endmodule
module AND2(input A, input B, output Y); assign Y = A & B; endmodule
module OR2(input A, input B, output Y); assign Y = A | B; endmodule
module XOR2(input A, input B, output Y); assign Y = A ^ B; endmodule
module XNOR2(input A, input B, output Y); assign Y = ~(A ^ B); endmodule
module NAND3(input A, input B, input C, output Y); assign Y = ~(A & B & C); endmodule
module NOR3(input A, input B, input C, output Y); assign Y = ~(A | B | C); endmodule
module AOI21(input A, input B, input C, output Y); assign Y = ~((A & B) | C); endmodule
module OAI21(input A, input B, input C, output Y); assign Y = ~((A | B) & C); endmodule
module MUX2(input A, input B, input S, output Y); assign Y = S ? B : A; endmodule
module DFF(input D, input CK, output reg Q);
`ifdef POWERUP_ZERO
    initial Q = 1'b0;      // a defined power-up state (real flip-flops power up to 0 or 1, unpredictably)
`endif
    always @(posedge CK) Q <= D;
endmodule
