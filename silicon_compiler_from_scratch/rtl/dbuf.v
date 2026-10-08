// Chapter 5: stream T tiles of TILE words from external memory, sum each tile, and report the sums, with the load of the next tile hidden behind the compute of the current one.
//   Two banks of the scratchpad hold two tiles. dbl = 1: tile n is loaded into bank n%2 as soon as that bank is EMPTY, while the other bank is being computed on (double buffering).
//   dbl = 0: a load may start only when nothing is full and nothing is computing (serial: load, compute, load, compute...).
//   A bank is marked empty only when the compute on it has FINISHED, so a load can never overwrite data that is still being read.
//   Compute reads one word every CPW cycles (CPW models how long the arithmetic takes per word) and adds it into a 32-bit sum.
module dbuf #(parameter TILE = 16, CPW = 1, AW = 8, XW = 16) (
    input clk, input rst, input start, input dbl, input [7:0] ntiles, input [XW-1:0] base,
    output req_valid, output [XW-1:0] req_addr, input resp_valid, input [31:0] resp_data,
    output reg done, output reg sum_valid, output reg [31:0] sum_data, output reg [31:0] cycles);
    // scratchpad and DMA
    wire sram_we; wire [AW-1:0] sram_waddr; wire [31:0] sram_wdata, rdata; wire [AW-1:0] raddr;
    sram #(.W(32), .AW(AW)) mem (.clk(clk), .we(sram_we), .waddr(sram_waddr), .wdata(sram_wdata), .raddr(raddr), .rdata(rdata));
    reg d_start; reg [XW-1:0] d_src; reg [AW-1:0] d_dst; wire d_busy, d_done;
    dma #(.AW(AW), .XW(XW)) engine (.clk(clk), .rst(rst), .start(d_start), .src(d_src), .dst(d_dst), .len(TILE[AW-1:0]), .busy(d_busy), .done(d_done),
        .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data), .sram_we(sram_we), .sram_waddr(sram_waddr), .sram_wdata(sram_wdata));
    // control state
    reg running; reg [7:0] ld_n, cp_n, sums_out; reg [1:0] full; reg ld_active; reg ld_bank;
    // compute state
    reg c_busy, c_bank, drain, rd_v; reg [AW-1:0] wi; reg [7:0] ph; reg [31:0] acc;
    wire c_issue = c_busy && !drain && ph == CPW - 1;                     // a word is read in the last cycle of its slot, so every CPW takes TILE*CPW cycles plus a fixed tail
    assign raddr = (c_bank ? TILE : 0) + wi;
    wire c_finish = c_busy && drain && !rd_v;
    // decisions made this cycle (functions of the state before the edge)
    wire [0:0] nb = ld_n[0];                                                       // bank the next load would use
    wire can_load = running && !ld_active && !d_busy && ld_n < ntiles && !full[nb] && (dbl || (full == 2'b00 && !c_busy));
    wire can_comp = running && !c_busy && full[cp_n[0]] && cp_n < ntiles;
    always @(posedge clk) begin
        d_start <= 1'b0; done <= 1'b0; sum_valid <= 1'b0; rd_v <= 1'b0;
        if (rst) begin running <= 0; ld_active <= 0; c_busy <= 0; full <= 0; cycles <= 0; end
        else begin
            if (start && !running) begin running <= 1; ld_n <= 0; cp_n <= 0; sums_out <= 0; full <= 0; ld_active <= 0; c_busy <= 0; cycles <= 0; end
            else if (running) begin
                cycles <= cycles + 1;
                // DMA side
                if (can_load) begin d_start <= 1'b1; d_src <= base + ld_n * TILE; d_dst <= (nb ? TILE : 0); ld_active <= 1; ld_bank <= nb; end
                if (d_done) begin full[ld_bank] <= 1'b1; ld_active <= 0; ld_n <= ld_n + 1; end
                // compute side
                if (can_comp) begin c_busy <= 1; c_bank <= cp_n[0]; wi <= 0; ph <= 0; drain <= 0; acc <= 0; end
                if (c_busy) begin
                    if (c_issue) rd_v <= 1'b1;
                    if (rd_v) acc <= acc + rdata;
                    if (!drain) begin
                        if (ph == CPW - 1) begin ph <= 0; if (wi == TILE - 1) drain <= 1; else wi <= wi + 1; end else ph <= ph + 1;
                    end
                    if (c_finish) begin
                        c_busy <= 0; full[c_bank] <= 1'b0; cp_n <= cp_n + 1; sum_valid <= 1'b1; sum_data <= acc; sums_out <= sums_out + 1;
                        if (sums_out == ntiles - 1) begin running <= 0; done <= 1'b1; end
                    end
                end
            end
        end
    end
endmodule
