// Chapter 2, running example B: push the worst-case product (-128 * -128 = +16384) into a wrapping MAC and a saturating MAC and print both accumulators at chosen counts. 2^31 / 16384 = 131072, so the 131072nd product is the one that does not fit.
`timescale 1ns/1ps
module mac_overflow_tb;
    reg clk = 0, rst = 1, clr = 0, en = 0; wire signed [31:0] w, s; integer n, k;
    mac #(.SAT(0)) uw (.clk(clk), .rst(rst), .clr(clr), .en(en), .a(-8'sd128), .b(-8'sd128), .acc(w));
    mac_sat        us (.clk(clk), .rst(rst), .clr(clr), .en(en), .a(-8'sd128), .b(-8'sd128), .acc(s));
    always #5 clk = ~clk;
    task run_to(input integer target);                         // keep adding until `target` products have been added in total
        begin while (n < target) begin @(negedge clk); en = 1; @(posedge clk); n = n + 1; end @(negedge clk); en = 0; #1; $display("O %0d %0d %0d", n, w, s); end
    endtask
    initial begin
        n = 0; @(posedge clk); #1; rst = 0;
        run_to(1); run_to(1000); run_to(131071); run_to(131072); run_to(131073); run_to(131074); run_to(262143); run_to(262144); run_to(262145);
        $finish;
    end
endmodule
