module appx_add8 (input [7:0] a, input [7:0] b, output [8:0] s); demo_add #(8) u (.a(a), .b(b), .s(s)); endmodule
module appx_ripple8 (input [7:0] a, input [7:0] b, output [8:0] s); demo_ripple #(8) u (.a(a), .b(b), .s(s)); endmodule
module appx_kogge8 (input [7:0] a, input [7:0] b, output [8:0] s); demo_kogge #(8) u (.a(a), .b(b), .s(s)); endmodule
