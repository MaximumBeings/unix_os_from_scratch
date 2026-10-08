module systolic_n1(input clk, input clr, input [7:0] a_edge, input [7:0] b_edge, output [31:0] c); systolic #(.N(1)) u(.*); endmodule
