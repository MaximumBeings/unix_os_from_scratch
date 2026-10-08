// Chapter 4: feed the array skewed matrices, wait exactly K+2N-2 cycles, and compare all N*N accumulators with the golden C. Size comes from -DNN= and -DKK= (default 4 and 7); vectors from out/systolic_vectors.hex.
`timescale 1ns/1ps
`ifndef NN
 `define NN 4
`endif
`ifndef KK
 `define KK 7
`endif
module systolic_tb;
    localparam N = `NN, K = `KK;
    reg clk = 0, clr = 0; reg [8*N-1:0] a_edge = 0, b_edge = 0; wire [32*N*N-1:0] c;
    systolic #(.N(N)) dut (.clk(clk), .clr(clr), .a_edge(a_edge), .b_edge(b_edge), .c(c));
    always #5 clk = ~clk;
    reg [31:0] mem [0:1048575]; integer ncases, cs, base, t, i, j, bad, hdrN, hdrK;
    reg signed [31:0] av, bv, got, want;
    initial begin
        $readmemh("out/systolic_vectors.hex", mem); ncases = mem[0]; hdrN = mem[1]; hdrK = mem[2]; bad = 0;
        if (hdrN != N || hdrK != K) begin $display("FAIL: vector file is for N=%0d K=%0d but the testbench is N=%0d K=%0d", hdrN, hdrK, N, K); $fatal(1, "size mismatch"); end
        for (cs = 0; cs < ncases; cs = cs + 1) begin
            base = 3 + cs * (N*K + K*N + N*N);
            @(negedge clk); clr = 1; a_edge = 0; b_edge = 0; @(negedge clk); clr = 0;
            for (t = 0; t < K + 2*N - 2; t = t + 1) begin
                for (i = 0; i < N; i = i + 1) begin
                    if (t - i >= 0 && t - i < K) av = mem[base + i*K + (t - i)]; else av = 0;
                    a_edge[8*i +: 8] = av[7:0];
                    if (t - i >= 0 && t - i < K) bv = mem[base + N*K + (t - i)*N + i]; else bv = 0;
                    b_edge[8*i +: 8] = bv[7:0];
                end
                @(negedge clk);
            end
            for (i = 0; i < N; i = i + 1) for (j = 0; j < N; j = j + 1) begin
                got = c[32*(i*N + j) +: 32]; want = mem[base + 2*N*K + i*N + j];
                if (got !== want) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH case %0d C[%0d][%0d]: got %0d want %0d", cs, i, j, got, want); end
            end
        end
        if (bad == 0) $display("PASS: N=%0d K=%0d, %0d matrix products match the golden model, each read after exactly %0d cycles (exit status 0)", N, K, ncases, K + 2*N - 2);
        else begin $display("FAIL: %0d wrong results", bad); $fatal(1, "systolic failed"); end
        $finish;
    end
endmodule
