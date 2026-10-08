// Chapter 7: the matrix unit of GA-2. It loads an M x K tile of A and a K x N tile of B from the scratchpad into small operand memories (one per row of A, one per column of B),
// streams them SKEWED through the 4 x 4 systolic array of Chapter 4, and writes the M x N int32 result back, one word per cycle.
//   MM a=dst b=A c=B f=M g=K h=N fl0=tb d[11:0]=lda d[23:12]=ldb x[11:0]=ldc.   M, N <= 4, K <= 64.
//   A[m][k] = spad[A + m*lda + k];  B[k][n] = spad[B + k*ldb + n] (tb = 0)  or  spad[B + n*ldb + k] (tb = 1);  C[m][n] -> spad[dst + m*ldc + n].
// Time: M*K (load A) + K*N (load B) + 1 + (K+M+N-2) (stream) + M*N (store) + a few cycles of control.
module ga2_mm (input clk, input rst, input start, input [127:0] ins, output reg busy, output reg done,
               output [11:0] ra, input [31:0] rd, output we, output [11:0] wa, output [31:0] wd);
    localparam IDLE = 0, LA = 1, LB = 2, PRE = 3, RUN = 4, OUT = 5, FIN = 6;
    reg [2:0] st; reg [11:0] dst, abase, bbase, lda, ldb, ldc; reg [3:0] M, N; reg [6:0] K; reg tb;
    reg [11:0] ptr, rowbase; reg [3:0] m, n; reg [6:0] k; reg iss, v; reg [3:0] m_d, n_d; reg [6:0] k_d;
    reg [7:0] t; reg [11:0] orow; reg [11:0] oaddr;
    // operand memories
    wire signed [7:0] a_rd [0:3]; wire signed [7:0] b_rd [0:3]; reg [5:0] a_ra [0:3]; reg [5:0] b_ra [0:3];
    genvar gi;
    generate for (gi = 0; gi < 4; gi = gi + 1) begin : opm
        rowmem am (.clk(clk), .we(v && st == LA && m_d == gi), .waddr(k_d[5:0]), .wdata(rd[7:0]), .raddr(a_ra[gi]), .rdata(a_rd[gi]));
        rowmem bm (.clk(clk), .we(v && st == LB && n_d == gi), .waddr(k_d[5:0]), .wdata(rd[7:0]), .raddr(b_ra[gi]), .rdata(b_rd[gi]));
    end endgenerate
    // the array
    reg [31:0] aedge, bedge; wire clr_s = (st == PRE); wire [511:0] cbus;   // clr is high during the PRE cycle, so the array is clean when the first operands arrive
    systolic #(.N(4)) arr (.clk(clk), .clr(clr_s), .a_edge(aedge), .b_edge(bedge), .c(cbus));
    integer ii;
    always @* begin
        aedge = 32'd0; bedge = 32'd0;
        for (ii = 0; ii < 4; ii = ii + 1) begin
            a_ra[ii] = t[5:0] - ii[5:0]; b_ra[ii] = t[5:0] - ii[5:0];
            if (st == RUN) begin
                if (ii < M && t >= ii && (t - ii) < K) aedge[8*ii +: 8] = a_rd[ii];
                if (ii < N && t >= ii && (t - ii) < K) bedge[8*ii +: 8] = b_rd[ii];
            end
        end
    end
    // scratchpad port
    wire [11:0] stepn = tb ? ldb : 12'd1;
    assign ra = ptr; assign we = (st == OUT); assign wa = oaddr; assign wd = cbus[32*(m*4 + n) +: 32];
    always @(posedge clk) begin
        done <= 1'b0;
        if (rst) begin busy <= 1'b0; st <= IDLE; v <= 1'b0; end
        else if (start && !busy) begin
            busy <= 1'b1; dst <= ins[119:108]; abase <= ins[103:92]; bbase <= ins[87:76]; M <= ins[41:38]; K <= ins[36:30]; N <= ins[25:22]; tb <= ins[18];
            lda <= ins[63:52]; ldb <= ins[75:64]; ldc <= ins[11:0];
            st <= LA; iss <= 1'b1; v <= 1'b0; m <= 0; k <= 0; ptr <= ins[103:92]; rowbase <= ins[103:92];
        end
        else if (busy) case (st)
            LA: begin
                v <= iss; m_d <= m; k_d <= k;
                if (iss) begin
                    if (k == K - 1) begin k <= 0; m <= m + 1; rowbase <= rowbase + lda; ptr <= rowbase + lda; if (m == M - 1) iss <= 1'b0; end
                    else begin k <= k + 1; ptr <= ptr + 1; end
                end
                if (!iss && !v) begin st <= LB; iss <= 1'b1; k <= 0; n <= 0; ptr <= bbase; rowbase <= bbase; end
            end
            LB: begin
                v <= iss; n_d <= n; k_d <= k;
                if (iss) begin
                    if (n == N - 1) begin n <= 0; k <= k + 1; rowbase <= rowbase + (tb ? 12'd1 : ldb); ptr <= rowbase + (tb ? 12'd1 : ldb); if (k == K - 1) iss <= 1'b0; end
                    else begin n <= n + 1; ptr <= ptr + stepn; end
                end
                if (!iss && !v) begin st <= PRE; end
            end
            PRE: begin t <= 0; st <= RUN; end
            RUN: begin
                t <= t + 1;
                if (t == K + M + N - 3) begin st <= OUT; m <= 0; n <= 0; orow <= dst; oaddr <= dst; end
            end
            OUT: begin
                if (n == N - 1) begin
                    n <= 0; m <= m + 1; orow <= orow + ldc; oaddr <= orow + ldc;
                    if (m == M - 1) begin st <= FIN; end
                end else begin n <= n + 1; oaddr <= oaddr + 1; end
            end
            FIN: begin busy <= 1'b0; done <= 1'b1; st <= IDLE; end
        endcase
    end
endmodule
