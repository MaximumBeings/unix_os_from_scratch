// Chapter 7: the softmax unit of GA-2: Chapter 6's algorithm, but reading its input from the scratchpad and writing its output there, so the row can have any length.
//   pass 1: read x, find the maximum m.   pass 2: read x again, e = exp_lut[m - x], write e to dst, sum += e.   divide: r = 2^38 / sum.   pass 3: read e from dst, write (e * r + 2^21) >> 22.
// SM a=dst b=src c=len. dst must equal src or not overlap it.
module ga2_sm (input clk, input rst, input start, input [127:0] ins, output reg busy, output reg done,
               output [11:0] ra, input [31:0] rd, output we, output [11:0] wa, output [31:0] wd);
    localparam P1 = 0, P2 = 1, DIVS = 2, WAITD = 3, P3 = 4;
    reg [2:0] st; reg [11:0] src, dst, len, i, j; reg v; reg signed [7:0] m; reg [31:0] sum; reg [39:0] r;
    wire signed [7:0] x = rd[7:0];
    wire [8:0] dfull = {m[7], m} - {x[7], x}; wire [15:0] e; exp_lut lut (.d(dfull[7:0]), .e(e));
    reg d_start; wire d_busy, d_done; wire [39:0] d_quot, d_rem;
    divu #(.W(40)) div (.clk(clk), .rst(rst), .start(d_start), .num(40'd1 << 38), .den({8'd0, sum}), .busy(d_busy), .done(d_done), .quot(d_quot), .rem(d_rem));
    wire [55:0] prod = rd[15:0] * r; wire [55:0] rounded = prod + 56'd2097152;
    assign ra = (st == P3) ? dst + i : src + i;
    assign we = v && (st == P2 || st == P3); assign wa = dst + j; assign wd = (st == P2) ? {16'd0, e} : {15'd0, rounded[38:22]};
    wire last = (j == len - 1);
    always @(posedge clk) begin
        done <= 1'b0; d_start <= 1'b0;
        if (rst) begin busy <= 1'b0; v <= 1'b0; end
        else if (start && !busy) begin busy <= 1'b1; dst <= ins[119:108]; src <= ins[103:92]; len <= ins[87:76]; i <= 0; j <= 0; v <= 1'b0; m <= -8'sd128; sum <= 0; st <= P1; end
        else if (busy) begin
            case (st)
                P1, P2, P3: begin
                    v <= (i < len); if (i < len) i <= i + 1;
                    if (v) begin
                        if (st == P1 && x > m) m <= x;
                        if (st == P2) sum <= sum + e;
                        j <= j + 1;
                        if (last) begin
                            v <= 1'b0; i <= 0; j <= 0;
                            if (st == P1) st <= P2; else if (st == P2) st <= DIVS; else begin busy <= 1'b0; done <= 1'b1; end
                        end
                    end
                end
                DIVS: begin d_start <= 1'b1; st <= WAITD; end
                WAITD: if (d_done) begin r <= d_quot; st <= P3; end
            endcase
        end
    end
endmodule
