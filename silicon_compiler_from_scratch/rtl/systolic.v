// Chapter 4: an output-stationary systolic array. N x N processing elements (PEs), each holding one element of the result C = A x B.
//   Row i of A enters from the left edge, column j of B enters from the top edge. Every PE multiplies the pair of values passing through it, adds the product to its own accumulator, and hands both values on (a to the right, b downwards) through a register.
//   The rows and columns are fed SKEWED: row i of A is delayed by i cycles and column j of B by j cycles, so that A[i][k] and B[k][j] reach PE(i,j) in the same cycle (cycle k+i+j).
//   clr = 1 for one cycle clears the accumulators AND the pass-through registers; zeros fed outside the matrix contribute nothing, so no enable is needed.
module pe (input clk, input clr, input signed [7:0] a_in, input signed [7:0] b_in, output reg signed [7:0] a_out, output reg signed [7:0] b_out, output reg signed [31:0] acc);
    always @(posedge clk) begin
        if (clr) begin a_out <= 8'sd0; b_out <= 8'sd0; acc <= 32'sd0; end
        else begin a_out <= a_in; b_out <= b_in; acc <= acc + a_in * b_in; end
    end
endmodule

module systolic #(parameter N = 4) (input clk, input clr, input [8*N-1:0] a_edge, input [8*N-1:0] b_edge, output [32*N*N-1:0] c);
    wire signed [7:0] aw [0:N-1][0:N];       // aw[i][j]: the a value entering PE(i,j) from the left
    wire signed [7:0] bw [0:N][0:N-1];       // bw[i][j]: the b value entering PE(i,j) from above
    genvar i, j;
    generate
        for (i = 0; i < N; i = i + 1) begin : row
            assign aw[i][0] = a_edge[8*i +: 8];
            assign bw[0][i] = b_edge[8*i +: 8];
            for (j = 0; j < N; j = j + 1) begin : col
                wire signed [31:0] acc;
                pe u (.clk(clk), .clr(clr), .a_in(aw[i][j]), .b_in(bw[i][j]), .a_out(aw[i][j+1]), .b_out(bw[i+1][j]), .acc(acc));
                assign c[32*(i*N + j) +: 32] = acc;
            end
        end
    endgenerate
endmodule
