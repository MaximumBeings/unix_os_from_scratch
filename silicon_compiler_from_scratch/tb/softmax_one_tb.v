// Chapter 6, running examples: run the softmax unit on ONE vector of N scores given on the command line (+bus=HEX, all N int8 scores packed with element 0 in the lowest byte; N from -DNN, default 4) and print the N probabilities (Q0.16) and the number of cycles it took.
`timescale 1ns/1ps
`ifndef NN
 `define NN 4
`endif
module softmax_one_tb;
    localparam N = `NN;
    reg clk = 0, rst = 1, start = 0; reg [8*N-1:0] x = 0; wire busy, done; wire [17*N-1:0] p; integer i, cyc;
    softmax #(.N(N)) dut (.clk(clk), .rst(rst), .start(start), .x(x), .busy(busy), .done(done), .p(p));
    always #5 clk = ~clk;
    initial begin
        if (!$value$plusargs("bus=%h", x)) x = 0;
        @(negedge clk); rst = 0; @(negedge clk); start = 1; cyc = 0; @(negedge clk); start = 0;
        while (!done) begin @(negedge clk); cyc = cyc + 1; end
        for (i = 0; i < N; i = i + 1) $display("P %0d %0d", i, p[17*i +: 17]);
        $display("CYCLES %0d", cyc); $finish;
    end
endmodule
