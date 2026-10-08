// Chapter 1, running example B: a 4-bit counter, the smallest circuit with MEMORY (four flip-flops).
//   Every rising clock edge: if rst is 1, q becomes 0 (reset wins over everything); otherwise, if en is 1, q increases by one, wrapping from 15 to 0; otherwise q holds.
//   wrap is 1 during the cycle in which the next edge will wrap q from 15 to 0 (a combinational output: it depends on q and en right now).
module counter4 (input clk, input rst, input en, output reg [3:0] q, output wrap);
    assign wrap = en && (q == 4'd15);
    always @(posedge clk) begin
        if (rst) q <= 4'd0;
        else if (en) q <= q + 4'd1;
    end
endmodule
