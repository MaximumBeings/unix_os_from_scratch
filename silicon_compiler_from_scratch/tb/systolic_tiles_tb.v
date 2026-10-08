// Chapter 4, running example B: run NCASES independent products on one N x N array (sizes from -DNN= -DKK=) and print every result matrix ("R case i j value"), so a script can stitch tiles together. Vectors come from out/systolic_tiles.hex in systolic_tb's format; the golden C in the file is ignored here.
`timescale 1ns/1ps
`ifndef NN
 `define NN 4
`endif
`ifndef KK
 `define KK 8
`endif
module systolic_tiles_tb;
    localparam N = `NN, K = `KK;
    reg clk = 0, clr = 0; reg [8*N-1:0] a_edge = 0, b_edge = 0; wire [32*N*N-1:0] c;
    systolic #(.N(N)) dut (.clk(clk), .clr(clr), .a_edge(a_edge), .b_edge(b_edge), .c(c));
    always #5 clk = ~clk;
    reg [31:0] mem [0:65535]; integer ncases, cs, base, t, i, j, cycles; reg signed [31:0] av, bv;
    initial begin
        $readmemh("out/systolic_tiles.hex", mem); ncases = mem[0]; cycles = 0;
        for (cs = 0; cs < ncases; cs = cs + 1) begin
            base = 3 + cs * (N*K + K*N + N*N);
            @(negedge clk); clr = 1; a_edge = 0; b_edge = 0; @(negedge clk); clr = 0;
            for (t = 0; t < K + 2*N - 2; t = t + 1) begin
                for (i = 0; i < N; i = i + 1) begin
                    av = (t - i >= 0 && t - i < K) ? mem[base + i*K + (t - i)] : 0; a_edge[8*i +: 8] = av[7:0];
                    bv = (t - i >= 0 && t - i < K) ? mem[base + N*K + (t - i)*N + i] : 0; b_edge[8*i +: 8] = bv[7:0];
                end
                @(negedge clk); cycles = cycles + 1;
            end
            for (i = 0; i < N; i = i + 1) for (j = 0; j < N; j = j + 1) $display("R %0d %0d %0d %0d", cs, i, j, $signed(c[32*(i*N + j) +: 32]));
        end
        $display("CYCLES %0d", cycles); $finish;
    end
endmodule
