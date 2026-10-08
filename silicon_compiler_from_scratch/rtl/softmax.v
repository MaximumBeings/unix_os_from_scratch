// Chapter 6: softmax of N signed 8-bit scores (each is a real number times 16: Q4.4), producing N probabilities in Q0.16 (65536 = 1.0), in fixed-point integers only.
//   p_i = exp(x_i - max) / sum_j exp(x_j - max)
// Steps: (1) find the maximum; (2) for each element look up e_i = exp(-(max - x_i)/16) in a 256-entry table and add it to the sum; (3) ONE division r = floor(2^38 / sum); (4) p_i = (e_i * r + 2^21) >> 22.
// Subtracting the maximum first keeps every exponent <= 0, so e_i <= 65535 and nothing overflows: the same trick as in floating-point softmax, here it is what makes 16-bit entries enough.
module softmax #(parameter N = 8) (input clk, input rst, input start, input [8*N-1:0] x, output reg busy, output reg done, output reg [17*N-1:0] p);
    reg signed [7:0] xs [0:N-1]; reg [15:0] ev [0:N-1];
    reg [2:0] state; reg [7:0] idx; reg signed [7:0] m; reg [31:0] sum; reg [39:0] r;
    localparam IDLE = 0, MAXS = 1, EXPS = 2, DIVS = 3, WAITD = 4, MULS = 5;
    wire [8:0] dfull = {m[7], m} - {xs[idx][7], xs[idx]};          // m - x, always >= 0 once m is the maximum
    wire [15:0] lut_e; exp_lut lut (.d(dfull[7:0]), .e(lut_e));
    reg d_start; wire d_busy, d_done; wire [39:0] d_quot, d_rem;
    divu #(.W(40)) div (.clk(clk), .rst(rst), .start(d_start), .num(40'd1 << 38), .den({8'd0, sum}), .busy(d_busy), .done(d_done), .quot(d_quot), .rem(d_rem));
    wire [55:0] prod = ev[idx] * r;
    wire [55:0] rounded = prod + 56'd2097152;                      // + 2^21
    integer k;
    always @(posedge clk) begin
        done <= 1'b0; d_start <= 1'b0;
        if (rst) begin state <= IDLE; busy <= 1'b0; end
        else case (state)
            IDLE: if (start) begin
                for (k = 0; k < N; k = k + 1) xs[k] <= x[8*k +: 8];
                busy <= 1'b1; idx <= 0; m <= -8'sd128; sum <= 0; state <= MAXS;
            end
            MAXS: begin if (xs[idx] > m) m <= xs[idx]; if (idx == N - 1) begin idx <= 0; state <= EXPS; end else idx <= idx + 1; end
            EXPS: begin ev[idx] <= lut_e; sum <= sum + lut_e; if (idx == N - 1) begin idx <= 0; state <= DIVS; end else idx <= idx + 1; end
            DIVS: begin d_start <= 1'b1; state <= WAITD; end
            WAITD: if (d_done) begin r <= d_quot; state <= MULS; idx <= 0; end
            MULS: begin
                p[17*idx +: 17] <= rounded[38:22];
                if (idx == N - 1) begin state <= IDLE; busy <= 1'b0; done <= 1'b1; end else idx <= idx + 1;
            end
        endcase
    end
endmodule
