// Chapter 5: an on-chip memory (a scratchpad). One write port and one read port; the read is SYNCHRONOUS: the address is presented in one cycle and the data appears in the next.
// If a read and a write hit the same address in the same cycle, the read returns the OLD value (read-before-write), which is what the real memory macros do and what the controller relies on never happening.
module sram #(parameter W = 32, AW = 8) (input clk, input we, input [AW-1:0] waddr, input [W-1:0] wdata, input [AW-1:0] raddr, output reg [W-1:0] rdata);
    reg [W-1:0] mem [0:(1<<AW)-1];
    always @(posedge clk) begin
        if (we) mem[waddr] <= wdata;
        rdata <= mem[raddr];
    end
endmodule
