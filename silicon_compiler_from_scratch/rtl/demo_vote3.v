// Background page demo: a three-input majority voter, written three ways, to show what a hardware description looks like. All three describe the same circuit.
module vote3_assign (input a, input b, input c, output y);          // 1. a continuous assignment: "y is always this expression of a, b, c"
    assign y = (a & b) | (a & c) | (b & c);
endmodule
module vote3_always (input a, input b, input c, output reg y);       // 2. a procedural block: the same function written like a small program
    always @* begin
        case ({a, b, c})
            3'b011, 3'b101, 3'b110, 3'b111: y = 1'b1;
            default: y = 1'b0;
        endcase
    end
endmodule
module vote3_gates (input a, input b, input c, output y);            // 3. structural: explicit gates and wires
    wire ab, ac, bc;
    and g1 (ab, a, b); and g2 (ac, a, c); and g3 (bc, b, c); or g4 (y, ab, ac, bc);
endmodule
// a clocked circuit: a register that remembers the last value of y, then the vote of the last three values (a 3-bit window)
module vote_seq (input clk, input rst, input d, output y);
    reg [2:0] w;
    always @(posedge clk) if (rst) w <= 3'b000; else w <= {w[1:0], d};   // shift register: "on every rising clock edge ..."
    vote3_assign v (.a(w[0]), .b(w[1]), .c(w[2]), .y(y));
endmodule
