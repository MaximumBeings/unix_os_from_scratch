module appx_add32 (input [31:0] a, input [31:0] b, output [32:0] s); demo_add #(32) u (.a(a), .b(b), .s(s)); endmodule
module appx_ripple32 (input [31:0] a, input [31:0] b, output [32:0] s); demo_ripple #(32) u (.a(a), .b(b), .s(s)); endmodule
module appx_kogge32 (input [31:0] a, input [31:0] b, output [32:0] s); demo_kogge #(32) u (.a(a), .b(b), .s(s)); endmodule
