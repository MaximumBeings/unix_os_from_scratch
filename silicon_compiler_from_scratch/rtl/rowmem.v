// Chapter 7: a small operand memory with an asynchronous read, used by the matrix unit to hold one row of A or one column of B.
module rowmem #(parameter DEPTH = 64) (input clk, input we, input [5:0] waddr, input signed [7:0] wdata, input [5:0] raddr, output signed [7:0] rdata);
    reg signed [7:0] mem [0:DEPTH-1];
    always @(posedge clk) if (we) mem[waddr] <= wdata;
    assign rdata = mem[raddr];
endmodule
