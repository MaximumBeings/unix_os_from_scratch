// Chapter 5: a DMA engine. It copies len words from external memory (word address src) into the scratchpad (word address dst) without the processor touching each word.
// External interface: every cycle the engine may present a request (req_valid, req_addr); the answer (resp_valid, resp_data) arrives a fixed number of cycles later, and requests are pipelined, so the engine keeps issuing without waiting.
// Cycles for one transfer = len (issue) + latency + a few cycles of control; that is the whole cost model, and the double-buffered controller exists to hide it.
module dma #(parameter AW = 8, XW = 16) (
    input clk, input rst, input start, input [XW-1:0] src, input [AW-1:0] dst, input [AW-1:0] len,
    output reg busy, output reg done,
    output req_valid, output [XW-1:0] req_addr, input resp_valid, input [31:0] resp_data,
    output sram_we, output [AW-1:0] sram_waddr, output [31:0] sram_wdata);
    reg [XW-1:0] src_r; reg [AW-1:0] dst_r, len_r, issued, recvd;
    assign req_valid = busy && issued != len_r;
    assign req_addr = src_r + issued;
    assign sram_we = resp_valid; assign sram_waddr = dst_r + recvd; assign sram_wdata = resp_data;
    always @(posedge clk) begin
        done <= 1'b0;
        if (rst) begin busy <= 1'b0; issued <= 0; recvd <= 0; end
        else if (start && !busy) begin busy <= 1'b1; src_r <= src; dst_r <= dst; len_r <= len; issued <= 0; recvd <= 0; end
        else if (busy) begin
            if (issued != len_r) issued <= issued + 1;
            if (resp_valid) begin
                recvd <= recvd + 1;
                if (recvd == len_r - 1) begin busy <= 1'b0; done <= 1'b1; end
            end
        end
    end
endmodule
