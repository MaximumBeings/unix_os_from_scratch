// Chapter 6: the divider against the golden model, including division by zero. It also checks that done arrives exactly W = 40 cycles after the start cycle, whatever the operands.
`timescale 1ns/1ps
module divu_tb;
    reg clk = 0, rst = 1, start = 0; reg [39:0] num = 0, den = 0; wire busy, done; wire [39:0] quot, rem;
    divu #(.W(40)) dut (.clk(clk), .rst(rst), .start(start), .num(num), .den(den), .busy(busy), .done(done), .quot(quot), .rem(rem));
    always #5 clk = ~clk;
    reg [159:0] vec [0:262143]; reg [159:0] v; integer i, n, bad, cyc;
    initial begin #1000000000; $display("FAIL: timeout"); $fatal(1, "timeout"); end
    initial begin
        $readmemh("out/divu_vectors.hex", vec); n = vec[0]; bad = 0;
        @(negedge clk); rst = 0;
        for (i = 1; i <= n; i = i + 1) begin
            v = vec[i]; @(negedge clk); num = v[159:120]; den = v[119:80]; start = 1; cyc = 0; @(negedge clk); start = 0;
            while (!done) begin @(negedge clk); cyc = cyc + 1; if (cyc == 5 && i % 2 == 1) begin num = ~num; den = ~den; start = 1; end else start = 0; end   // on odd cases, a second start with garbage operands in mid-division must be ignored
            start = 0;
            if (quot !== v[79:40] || rem !== v[39:0] || cyc != 40) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH %0d/%0d: got q=%0d r=%0d after %0d cycles, want q=%0d r=%0d, 40", num, den, quot, rem, cyc, v[79:40], v[39:0]); end
        end
        if (bad == 0) $display("PASS: %0d divisions match the golden model, each in the same number of cycles (exit status 0)", n); else begin $display("FAIL: %0d wrong", bad); $fatal(1, "divu failed"); end
        $finish;
    end
endmodule
