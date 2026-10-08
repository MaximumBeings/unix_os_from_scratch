// Chapter 5: a model of external memory (DRAM): requests are accepted every cycle and each answer appears LAT cycles after its request. A testbench part, not a design.
module extmem #(parameter LAT = 4, XW = 16, DEPTH = 4096) (input clk, input req_valid, input [XW-1:0] req_addr, output resp_valid, output [31:0] resp_data);
    reg [31:0] mem [0:DEPTH-1];
    reg v [0:LAT-1]; reg [31:0] d [0:LAT-1]; integer k;
    always @(posedge clk) begin
        v[0] <= req_valid; d[0] <= mem[req_addr];
        for (k = 1; k < LAT; k = k + 1) begin v[k] <= v[k-1]; d[k] <= d[k-1]; end
    end
    assign resp_valid = v[LAT-1]; assign resp_data = d[LAT-1];
    initial for (k = 0; k < LAT; k = k + 1) v[k] = 0;
endmodule
