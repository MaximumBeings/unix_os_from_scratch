// Chapter 6: the softmax unit against the golden fixed-point model for N = -DNN (default 8): every output word, and the cycle count, which must be the same for every input.
`timescale 1ns/1ps
`ifndef NN
 `define NN 8
`endif
module softmax_tb;
    localparam N = `NN;
    reg clk = 0, rst = 1, start = 0; reg [8*N-1:0] x = 0; wire busy, done; wire [17*N-1:0] p;
    softmax #(.N(N)) dut (.clk(clk), .rst(rst), .start(start), .x(x), .busy(busy), .done(done), .p(p));
    always #5 clk = ~clk;
    reg [31:0] mem [0:1048575]; integer ncases, hdrN, cs, i, bad, base, cyc, first_cyc; reg [31:0] want; reg [16:0] got; reg signed [31:0] xv;
    initial begin #1000000000; $display("FAIL: timeout"); $fatal(1, "timeout"); end
    initial begin
        $readmemh("out/softmax_vectors.hex", mem); ncases = mem[0]; hdrN = mem[1]; bad = 0; first_cyc = -1;
        if (hdrN != N) begin $display("FAIL: vectors are for N=%0d, testbench is N=%0d", hdrN, N); $fatal(1, "size mismatch"); end
        @(negedge clk); rst = 0;
        for (cs = 0; cs < ncases; cs = cs + 1) begin
            base = 2 + cs * 2 * N;
            for (i = 0; i < N; i = i + 1) begin xv = mem[base + i]; x[8*i +: 8] = xv[7:0]; end
            @(negedge clk); start = 1; cyc = 0; @(negedge clk); start = 0;
            while (!done) begin @(negedge clk); cyc = cyc + 1; end
            if (first_cyc < 0) first_cyc = cyc;
            if (cyc != first_cyc) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH case %0d: %0d cycles, the first case took %0d", cs, cyc, first_cyc); end
            for (i = 0; i < N; i = i + 1) begin
                got = p[17*i +: 17]; want = mem[base + N + i];
                if (got !== want[16:0]) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH case %0d p[%0d]: got %0d want %0d", cs, i, got, want); end
            end
        end
        if (bad == 0) $display("PASS: N=%0d, %0d softmax cases match the golden model; every case took %0d cycles (exit status 0)", N, ncases, first_cyc);
        else begin $display("FAIL: %0d wrong", bad); $fatal(1, "softmax failed"); end
        $finish;
    end
endmodule
