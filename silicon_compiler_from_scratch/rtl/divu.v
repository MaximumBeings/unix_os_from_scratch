// Chapter 6: an unsigned divider, one quotient bit per clock (restoring division: shift the remainder left, bring in the next bit of the numerator, subtract the divisor if it fits).
// quot = num / den and rem = num % den; done pulses W cycles after the start cycle. Dividing by zero is defined, not undefined: quot = all ones, rem = num (what the loop naturally does when the subtraction always "fits").
// This is the reciprocal unit of softmax: a divide is slow and large in hardware, so the chip does ONE division per row and multiplies the rest.
module divu #(parameter W = 40) (input clk, input rst, input start, input [W-1:0] num, input [W-1:0] den,
                                 output reg busy, output reg done, output reg [W-1:0] quot, output reg [W-1:0] rem);
    reg [W-1:0] n_r, d_r; reg [W:0] r; reg [W-1:0] q; reg [7:0] cnt;
    wire [W:0] shifted = {r[W-1:0], n_r[W-1]};
    wire fits = shifted >= {1'b0, d_r};
    wire [W:0] r_next = fits ? shifted - {1'b0, d_r} : shifted;
    wire [W-1:0] q_next = {q[W-2:0], fits};
    always @(posedge clk) begin
        done <= 1'b0;
        if (rst) busy <= 1'b0;
        else if (start && !busy) begin busy <= 1'b1; n_r <= num; d_r <= den; r <= 0; q <= 0; cnt <= W; end
        else if (busy) begin
            r <= r_next; q <= q_next; n_r <= n_r << 1; cnt <= cnt - 1;
            if (cnt == 1) begin busy <= 1'b0; done <= 1'b1; quot <= q_next; rem <= r_next[W-1:0]; end
        end
    end
endmodule
