// Chapter 2: the MAC unit (wrapping and saturating versions side by side) against the golden clock-by-clock vectors, including runs of up to 32,768 identical cycles that push the accumulator past both ends of the 32-bit range.
`timescale 1ns/1ps
module mac_tb;
    reg clk = 0, rst = 1, clr = 0, en = 0; reg signed [7:0] a = 0, b = 0; wire signed [31:0] accw, accs;
    mac #(.SAT(0)) dw(.clk(clk), .rst(rst), .clr(clr), .en(en), .a(a), .b(b), .acc(accw));
    mac #(.SAT(1)) ds(.clk(clk), .rst(rst), .clr(clr), .en(en), .a(a), .b(b), .acc(accs));
    always #5 clk = ~clk;
    reg [101:0] vec [0:4095]; integer i, k, rep, badw, bads, checks, nrows; reg [101:0] v;
    initial begin
        $readmemh("out/mac_vectors.hex", vec); badw = 0; bads = 0; checks = 0;
        @(posedge clk); #1 rst = 0;
        nrows = vec[0];
        for (i = 1; i <= nrows; i = i + 1) begin   // word 0 is the row count (a simulator that has only 0 and 1, like Verilator, cannot be asked to find the end of the data by looking for an unknown)
            v = vec[i]; rep = v[101:82]; clr = v[81]; en = v[80]; a = v[79:72]; b = v[71:64];
            for (k = 0; k < rep; k = k + 1) begin @(posedge clk); #1 clr = 1'b0; end   // clr applies to the first cycle of a repeated run only
            checks = checks + 1;
            if (accw !== v[63:32]) begin badw = badw + 1; if (badw <= 3) $display("wrap MISMATCH at row %0d: got %0d want %0d", i, accw, $signed(v[63:32])); end
            if (accs !== v[31:0]) begin bads = bads + 1; if (bads <= 3) $display("sat MISMATCH at row %0d: got %0d want %0d", i, accs, $signed(v[31:0])); end
        end
        if (badw == 0 && bads == 0) $display("PASS: wrapping and saturating MACs match the golden model at all %0d checkpoints", checks);
        else begin $display("FAIL: wrap %0d, sat %0d checkpoints differ", badw, bads); $fatal(1, "mac failed"); end
        $finish;
    end
endmodule
