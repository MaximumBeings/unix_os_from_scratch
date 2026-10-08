// Chapter 1, running example B: a short, readable run of the counter, dumped as a waveform: reset, count to a wrap, hold, reset while enabled.
`timescale 1ns/1ns
module counter4_wave_tb;
    reg clk = 0, rst = 0, en = 0; wire [3:0] q; wire wrap; integer i;
    counter4 dut (.clk(clk), .rst(rst), .en(en), .q(q), .wrap(wrap));
    always #5 clk = ~clk;                                    // period 10: rising edges at 5, 15, 25, ...
    initial begin
        $dumpfile("out/counter4.vcd"); $dumpvars(0, counter4_wave_tb);
        rst = 1; en = 0; #20; rst = 0; en = 1; #170;       // reset for two edges, then count: passes 15 -> 0 once
        en = 0; #40; en = 1; #30; rst = 1; #10; rst = 0; #30;   // hold four edges; count; reset while enabled; count again
        $finish;
    end
endmodule
