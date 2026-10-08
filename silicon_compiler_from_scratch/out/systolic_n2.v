module systolic_n2(input clk, input clr, input [15:0] a_edge, input [15:0] b_edge, output [127:0] c); systolic #(.N(2)) u(.*); endmodule
