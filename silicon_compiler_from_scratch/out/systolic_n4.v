module systolic_n4(input clk, input clr, input [31:0] a_edge, input [31:0] b_edge, output [511:0] c); systolic #(.N(4)) u(.*); endmodule
