module systolic_n8(input clk, input clr, input [63:0] a_edge, input [63:0] b_edge, output [2047:0] c); systolic #(.N(8)) u(.*); endmodule
