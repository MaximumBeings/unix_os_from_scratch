// Chapter 1, running example A: apply 0111 + 0001 (the carry has to travel through all four stages) and dump every signal change to a VCD file, a standard waveform format.
`timescale 1ns/1ns
module ripple_tb;
    reg [3:0] a, b; reg cin; wire [3:0] sum; wire cout; wire [4:0] c = {dut.stage[3].fa.cout, dut.stage[2].fa.cout, dut.stage[1].fa.cout, dut.stage[0].fa.cout, cin};
    ripple4 dut (.a(a), .b(b), .cin(cin), .sum(sum), .cout(cout));
    initial begin
        $dumpfile("out/ripple.vcd"); $dumpvars(0, ripple_tb);
        a = 4'b0000; b = 4'b0000; cin = 0; #10;
        a = 4'b0111; b = 4'b0001; #10;           // 7 + 1: the carry ripples through stages 0, 1, 2
        a = 4'b1111; b = 4'b0001; #10;           // 15 + 1: through all four, out of the top
        $finish;
    end
endmodule
