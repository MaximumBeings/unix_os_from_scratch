// Chapter 7: the simple units of GA-2. Each takes the 128-bit instruction when `start` pulses, owns the scratchpad ports while `busy`, and pulses `done` when its last word is written.
// Scratchpad reads are synchronous (address now, data next cycle), so every unit is a small PIPELINE: issue read i in one cycle, process word i in the next.
// Field positions: a=ins[123:108] b=ins[107:92] c=ins[91:76] d=ins[75:52] e=ins[51:46] fl=ins[21:18] (see model/ga2_isa.py).

// ST: scratchpad -> external memory.  ST a=src b=ext dst c=len
module ga2_st (input clk, input rst, input start, input [127:0] ins, output reg busy, output reg done,
               output [11:0] ra, input [31:0] rd, output wr_valid, output [15:0] wr_addr, output [31:0] wr_data);
    reg [11:0] src, len, i, j; reg [15:0] dst; reg v;
    assign ra = src + i; assign wr_valid = v; assign wr_addr = dst + j; assign wr_data = rd;
    always @(posedge clk) begin
        done <= 1'b0;
        if (rst) begin busy <= 1'b0; v <= 1'b0; end
        else if (start && !busy) begin busy <= 1'b1; src <= ins[119:108]; dst <= ins[107:92]; len <= ins[87:76]; i <= 0; j <= 0; v <= 1'b0; end
        else if (busy) begin
            v <= (i < len); if (i < len) i <= i + 1;
            if (v) begin j <= j + 1; if (j == len - 1) begin busy <= 1'b0; done <= 1'b1; v <= 1'b0; end end
        end
    end
endmodule

// RQ: requantize int32 words to int8 words (Chapter 3).  RQ a=dst b=src c=len d=mantissa e=shift fl0=relu
module ga2_rq (input clk, input rst, input start, input [127:0] ins, output reg busy, output reg done,
               output [11:0] ra, input [31:0] rd, output we, output [11:0] wa, output [31:0] wd);
    reg [11:0] src, dst, len, i, j; reg [23:0] m; reg [5:0] s; reg relu; reg v;
    wire signed [7:0] q; requant rqu (.acc(rd), .m(m), .s(s), .relu(relu), .q(q));
    assign ra = src + i; assign we = v; assign wa = dst + j; assign wd = {{24{q[7]}}, q};
    always @(posedge clk) begin
        done <= 1'b0;
        if (rst) begin busy <= 1'b0; v <= 1'b0; end
        else if (start && !busy) begin busy <= 1'b1; dst <= ins[119:108]; src <= ins[103:92]; len <= ins[87:76]; m <= ins[75:52]; s <= ins[51:46]; relu <= ins[18]; i <= 0; j <= 0; v <= 1'b0; end
        else if (busy) begin
            v <= (i < len); if (i < len) i <= i + 1;
            if (v) begin j <= j + 1; if (j == len - 1) begin busy <= 1'b0; done <= 1'b1; v <= 1'b0; end end
        end
    end
endmodule

// VADD: saturating int8 add of two vectors, results clamped to -127..127.  VADD a=dst b=src1 c=src2 d[15:0]=len.  One scratchpad read port, so each element takes two cycles.
module ga2_vadd (input clk, input rst, input start, input [127:0] ins, output reg busy, output reg done,
                 output [11:0] ra, input [31:0] rd, output we, output [11:0] wa, output [31:0] wd);
    reg [11:0] s1, s2, dst, j; reg [15:0] len; reg [16:0] cnt; reg v, par; reg signed [7:0] x;
    assign ra = cnt[0] ? (s2 + cnt[16:1]) : (s1 + cnt[16:1]);
    wire signed [8:0] sum = {x[7], x} + {rd[7], rd[7:0]};
    wire signed [7:0] r = (sum > 9'sd127) ? 8'sd127 : (sum < -9'sd127) ? -8'sd127 : sum[7:0];
    assign we = v && par; assign wa = dst + j; assign wd = {{24{r[7]}}, r};
    always @(posedge clk) begin
        done <= 1'b0;
        if (rst) begin busy <= 1'b0; v <= 1'b0; end
        else if (start && !busy) begin busy <= 1'b1; dst <= ins[119:108]; s1 <= ins[103:92]; s2 <= ins[87:76]; len <= ins[67:52]; cnt <= 0; j <= 0; v <= 1'b0; end
        else if (busy) begin
            v <= (cnt < {len, 1'b0}); par <= cnt[0]; if (cnt < {len, 1'b0}) cnt <= cnt + 1;
            if (v && !par) x <= rd[7:0];
            if (v && par) begin j <= j + 1; if (j == len - 1) begin busy <= 1'b0; done <= 1'b1; v <= 1'b0; end end
        end
    end
endmodule

// AMAX: index of the first maximum of len signed 32-bit words; the index is written to dst.  AMAX a=dst b=src c=len
module ga2_amax (input clk, input rst, input start, input [127:0] ins, output reg busy, output reg done,
                 output [11:0] ra, input [31:0] rd, output we, output [11:0] wa, output [31:0] wd);
    reg [11:0] src, dst, len, i, j, bidx; reg signed [31:0] best; reg v, fin;
    assign ra = src + i; assign we = fin; assign wa = dst; assign wd = {20'd0, bidx};
    always @(posedge clk) begin
        done <= 1'b0;
        if (rst) begin busy <= 1'b0; v <= 1'b0; fin <= 1'b0; end
        else if (start && !busy) begin busy <= 1'b1; dst <= ins[119:108]; src <= ins[103:92]; len <= ins[87:76]; i <= 0; j <= 0; v <= 1'b0; fin <= 1'b0; end
        else if (busy) begin
            if (fin) begin fin <= 1'b0; busy <= 1'b0; done <= 1'b1; end
            else begin
                v <= (i < len); if (i < len) i <= i + 1;
                if (v) begin
                    if (j == 0 || $signed(rd) > best) begin best <= rd; bidx <= j; end
                    j <= j + 1; if (j == len - 1) fin <= 1'b1;
                end
            end
        end
    end
endmodule

// UNPACK (Chapter 13): expand packed 4-bit weights. Each scratchpad word holds eight signed 4-bit fields (field j in bits 4j+3..4j); field j of source word i becomes the sign-extended word dst[8i + j].
// UNPACK a=dst b=src c=number of packed source words.  dst and src must not overlap.  The scratchpad has one write port, so a source word costs 10 cycles: 1 to read it, 1 to latch it, 8 to write its fields.
module ga2_unp (input clk, input rst, input start, input [127:0] ins, output reg busy, output reg done,
                output [11:0] ra, input [31:0] rd, output we, output [11:0] wa, output [31:0] wd);
    localparam S_RD = 0, S_LAT = 1, S_WR = 2;
    reg [1:0] st; reg [11:0] src, dst, len, i; reg [2:0] j; reg [31:0] w;
    assign ra = src + i; assign we = busy && st == S_WR; assign wa = dst + {i, 3'b000} + j;
    wire [3:0] nib = w[4*j +: 4]; assign wd = {{28{nib[3]}}, nib};
    always @(posedge clk) begin
        done <= 1'b0;
        if (rst) begin busy <= 1'b0; st <= S_RD; end
        else if (start && !busy) begin busy <= 1'b1; dst <= ins[119:108]; src <= ins[103:92]; len <= ins[87:76]; i <= 0; j <= 0; st <= S_RD; end
        else if (busy) case (st)
            S_RD: st <= S_LAT;
            S_LAT: begin w <= rd; j <= 0; st <= S_WR; end
            S_WR: begin
                j <= j + 1;
                if (j == 7) begin
                    if (i == len - 1) begin busy <= 1'b0; done <= 1'b1; end else begin i <= i + 1; st <= S_RD; end
                end
            end
        endcase
    end
endmodule
