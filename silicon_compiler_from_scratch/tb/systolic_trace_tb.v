// Chapter 4, running example A: a 3 x 3 array multiplies A (3 x 4) by B (4 x 3). After every clock edge the testbench prints all nine accumulators ("F cycle acc00 acc01 ... acc22"), so the diamond can be watched filling and draining. The skew is applied here, exactly as in systolic_tb.v.
`timescale 1ns/1ps
module systolic_trace_tb;
    localparam N = 3, K = 4;
    reg clk = 0, clr = 0; reg [8*N-1:0] a_edge = 0, b_edge = 0; wire [32*N*N-1:0] c;
    systolic #(.N(N)) dut (.clk(clk), .clr(clr), .a_edge(a_edge), .b_edge(b_edge), .c(c));
    always #5 clk = ~clk;
    reg signed [7:0] A [0:N-1][0:K-1]; reg signed [7:0] B [0:K-1][0:N-1]; integer t, i, k, j; reg signed [7:0] av, bv;
    initial begin
        A[0][0]=1; A[0][1]=2; A[0][2]=3;  A[0][3]=4;      B[0][0]=1; B[0][1]=0; B[0][2]=2;
        A[1][0]=0; A[1][1]=-1; A[1][2]=2; A[1][3]=1;      B[1][0]=2; B[1][1]=1; B[1][2]=0;
        A[2][0]=5; A[2][1]=1; A[2][2]=-2; A[2][3]=0;      B[2][0]=0; B[2][1]=-3; B[2][2]=1;
                                                          B[3][0]=-1; B[3][1]=2; B[3][2]=1;
        @(negedge clk); clr = 1; @(negedge clk); clr = 0;
        for (t = 0; t < K + 2*N - 2 + 2; t = t + 1) begin
            for (i = 0; i < N; i = i + 1) begin
                av = (t - i >= 0 && t - i < K) ? A[i][t - i] : 8'sd0; a_edge[8*i +: 8] = av;                  // row i of A, delayed by i cycles
                bv = (t - i >= 0 && t - i < K) ? B[t - i][i] : 8'sd0; b_edge[8*i +: 8] = bv;                  // column i of B, delayed by i cycles
            end
            @(posedge clk); #1;
            $write("F %0d", t); for (i = 0; i < N*N; i = i + 1) $write(" %0d", $signed(c[32*i +: 32])); $write("\n");
            @(negedge clk);
        end
        $finish;
    end
endmodule
