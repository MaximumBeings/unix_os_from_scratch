// Appendix B: the pieces of the Verilog primer, small enough to read in one sitting.
// 1. A synchronous Gray-code counter: only one output bit changes per step. Clocked block, non-blocking assignments, synchronous reset, enable.
module gray_cnt #(parameter W = 4) (input clk, input rst, input en, output [W-1:0] g);
    reg [W-1:0] b;
    always @(posedge clk) if (rst) b <= 0; else if (en) b <= b + 1'b1;
    assign g = b ^ (b >> 1);                                    // binary to Gray: a combinational continuous assignment
endmodule
// 2. Swapping two registers. With non-blocking assignments (<=) both right-hand sides are read before either register changes: a true swap.
module swap_nb (input clk, input rst, output reg [3:0] a, output reg [3:0] b);
    always @(posedge clk) if (rst) begin a <= 4'd1; b <= 4'd2; end else begin a <= b; b <= a; end
endmodule
// With blocking assignments (=) the second statement sees the new value of a: both registers end up equal. A classic mistake in clocked blocks.
module swap_blocking (input clk, input rst, output reg [3:0] a, output reg [3:0] b);
    always @(posedge clk) if (rst) begin a = 4'd1; b = 4'd2; end else begin a = b; b = a; end
endmodule
// 3. A combinational block that forgets a case: when sel == 3 the output must hold its old value, so synthesis inserts a LATCH (a memory element nobody asked for).
module mux_latch (input [1:0] sel, input [3:0] d0, input [3:0] d1, input [3:0] d2, output reg [3:0] y);
    always @* case (sel) 2'd0: y = d0; 2'd1: y = d1; 2'd2: y = d2; endcase
endmodule
// The repaired version: every case assigned (a default).
module mux_ok (input [1:0] sel, input [3:0] d0, input [3:0] d1, input [3:0] d2, output reg [3:0] y);
    always @* case (sel) 2'd0: y = d0; 2'd1: y = d1; 2'd2: y = d2; default: y = 4'd0; endcase
endmodule
// 4. Width and sign: an 8-bit signed value extended to 12 bits. Sign extension replicates the top bit; zero extension does not.
module extend (input [7:0] x, output [11:0] sext, output [11:0] zext);
    assign sext = {{4{x[7]}}, x};
    assign zext = {4'b0000, x};
endmodule
