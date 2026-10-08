// Chapter 6: all 256 entries of the exp table against the golden table.
module exp_lut_tb;
    reg [7:0] d; wire [15:0] e; reg [15:0] want [0:255]; integer i, bad;
    exp_lut dut (.d(d), .e(e));
    initial begin
        $readmemh("out/exp_table.hex", want); bad = 0;
        for (i = 0; i < 256; i = i + 1) begin d = i; #1; if (e !== want[i]) begin bad = bad + 1; if (bad <= 5) $display("MISMATCH d=%0d: got %0d want %0d", i, e, want[i]); end end
        if (bad == 0) $display("PASS: all 256 table entries match the golden table (exit status 0)"); else begin $display("FAIL: %0d entries differ", bad); $fatal(1, "exp_lut failed"); end
        $finish;
    end
endmodule
