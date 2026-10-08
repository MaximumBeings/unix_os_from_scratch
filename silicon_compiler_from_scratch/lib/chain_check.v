module chain(input a, input clk, output y, output z);
  wire w1, w2, q;
  INV i1(.A(a), .Y(w1)); NAND2 n1(.A(w1), .B(a), .Y(w2)); INV i2(.A(w2), .Y(y));
  DFF f(.D(w2), .CK(clk), .Q(q)); INV i3(.A(q), .Y(z));
endmodule
