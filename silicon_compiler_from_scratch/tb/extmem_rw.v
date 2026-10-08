// Chapter 7: external memory for GA-2's testbench: a pipelined read port with LAT cycles of latency and a write port that takes effect at once, and a reset that discards requests still in flight. Words 0..DEPTH-1. A testbench part, not a design.
module extmem_rw #(parameter LAT = 8, DEPTH = 2048) (input clk, input rst, input req_valid, input [15:0] req_addr, output resp_valid, output [31:0] resp_data, input wr_valid, input [15:0] wr_addr, input [31:0] wr_data);
    reg [31:0] mem [0:DEPTH-1]; reg v [0:LAT-1]; reg [31:0] d [0:LAT-1]; integer k;
    always @(posedge clk) begin
        v[0] <= req_valid && !rst; d[0] <= (req_addr < DEPTH) ? mem[req_addr] : 32'hDEADBEEF;      // a reset clears every request in flight
        for (k = 1; k < LAT; k = k + 1) begin v[k] <= v[k-1] && !rst; d[k] <= d[k-1]; end
        if (wr_valid && wr_addr < DEPTH) mem[wr_addr] <= wr_data;
    end
    assign resp_valid = v[LAT-1]; assign resp_data = d[LAT-1];
    initial for (k = 0; k < LAT; k = k + 1) v[k] = 0;
endmodule
