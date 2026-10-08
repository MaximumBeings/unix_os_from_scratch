// Chapter 7: GA-2, the whole accelerator: a sequencer that fetches one 128-bit instruction at a time, hands it to the unit it names, waits for that unit to finish, and moves on.
//   Units: LD (the DMA of Chapter 5), ST, MM (the systolic array of Chapter 4), RQ (Chapter 3), SM (Chapter 6), VADD, AMAX.   Memory: one 4096-word scratchpad; external memory through a read port (latency set by the outside) and a write port.
//   The program lives outside (a ROM answering imem_addr -> imem_data in the same cycle). A zero word is HALT.
// Per instruction: 1 cycle FETCH, 1 cycle ISSUE, then the unit's busy time. Counters (cleared by `start`) give the profile used in Chapter 9.
module ga2 (input clk, input rst, input start, output reg halted,
            output [15:0] imem_addr, input [127:0] imem_data,
            output req_valid, output [15:0] req_addr, input resp_valid, input [31:0] resp_data,
            output wr_valid, output [15:0] wr_addr, output [31:0] wr_data,
            output reg [31:0] cyc_total, output reg [31:0] cyc_mm, output reg [31:0] cyc_dma, output reg [31:0] cyc_vec, output reg [31:0] n_inst);
    localparam IDLE = 0, FETCH = 1, ISSUE = 2, WAIT = 3, HALT = 4;
    localparam OP_HALT = 0, OP_LD = 1, OP_ST = 2, OP_MM = 3, OP_RQ = 4, OP_SM = 5, OP_VADD = 6, OP_AMAX = 7, OP_UNPACK = 8;
    reg [2:0] st; reg [15:0] pc; reg [127:0] ins; wire [3:0] op = ins[127:124];
    assign imem_addr = pc;
    // scratchpad and the unit ports
    wire sp_we; wire [11:0] sp_wa; wire [31:0] sp_wd; wire [11:0] sp_ra; wire [31:0] sp_rd;
    sram #(.W(32), .AW(12)) sp (.clk(clk), .we(sp_we), .waddr(sp_wa), .wdata(sp_wd), .raddr(sp_ra), .rdata(sp_rd));
    wire go = (st == ISSUE);
    wire ld_busy, ld_done, st_busy, st_done, mm_busy, mm_done, rq_busy, rq_done, sm_busy, sm_done, va_busy, va_done, am_busy, am_done;
    wire ld_we; wire [11:0] ld_wa; wire [31:0] ld_wd;
    dma #(.AW(12), .XW(16)) u_ld (.clk(clk), .rst(rst), .start(go && op == OP_LD), .src(ins[107:92]), .dst(ins[119:108]), .len(ins[87:76]), .busy(ld_busy), .done(ld_done),
        .req_valid(req_valid), .req_addr(req_addr), .resp_valid(resp_valid), .resp_data(resp_data), .sram_we(ld_we), .sram_waddr(ld_wa), .sram_wdata(ld_wd));
    wire [11:0] st_ra; ga2_st u_st (.clk(clk), .rst(rst), .start(go && op == OP_ST), .ins(ins), .busy(st_busy), .done(st_done), .ra(st_ra), .rd(sp_rd), .wr_valid(wr_valid), .wr_addr(wr_addr), .wr_data(wr_data));
    wire mm_we; wire [11:0] mm_ra, mm_wa; wire [31:0] mm_wd;
    ga2_mm u_mm (.clk(clk), .rst(rst), .start(go && op == OP_MM), .ins(ins), .busy(mm_busy), .done(mm_done), .ra(mm_ra), .rd(sp_rd), .we(mm_we), .wa(mm_wa), .wd(mm_wd));
    wire rq_we; wire [11:0] rq_ra, rq_wa; wire [31:0] rq_wd;
    ga2_rq u_rq (.clk(clk), .rst(rst), .start(go && op == OP_RQ), .ins(ins), .busy(rq_busy), .done(rq_done), .ra(rq_ra), .rd(sp_rd), .we(rq_we), .wa(rq_wa), .wd(rq_wd));
    wire sm_we; wire [11:0] sm_ra, sm_wa; wire [31:0] sm_wd;
    ga2_sm u_sm (.clk(clk), .rst(rst), .start(go && op == OP_SM), .ins(ins), .busy(sm_busy), .done(sm_done), .ra(sm_ra), .rd(sp_rd), .we(sm_we), .wa(sm_wa), .wd(sm_wd));
    wire va_we; wire [11:0] va_ra, va_wa; wire [31:0] va_wd;
    ga2_vadd u_va (.clk(clk), .rst(rst), .start(go && op == OP_VADD), .ins(ins), .busy(va_busy), .done(va_done), .ra(va_ra), .rd(sp_rd), .we(va_we), .wa(va_wa), .wd(va_wd));
    wire am_we; wire [11:0] am_ra, am_wa; wire [31:0] am_wd;
    ga2_amax u_am (.clk(clk), .rst(rst), .start(go && op == OP_AMAX), .ins(ins), .busy(am_busy), .done(am_done), .ra(am_ra), .rd(sp_rd), .we(am_we), .wa(am_wa), .wd(am_wd));
    wire un_we; wire [11:0] un_ra, un_wa; wire [31:0] un_wd; wire un_busy, un_done;
    ga2_unp u_un (.clk(clk), .rst(rst), .start(go && op == OP_UNPACK), .ins(ins), .busy(un_busy), .done(un_done), .ra(un_ra), .rd(sp_rd), .we(un_we), .wa(un_wa), .wd(un_wd));
    // the scratchpad ports belong to the unit named by the instruction in flight
    reg [3:0] cur;
    assign sp_ra = (cur == OP_ST) ? st_ra : (cur == OP_MM) ? mm_ra : (cur == OP_RQ) ? rq_ra : (cur == OP_SM) ? sm_ra : (cur == OP_VADD) ? va_ra : (cur == OP_AMAX) ? am_ra : (cur == OP_UNPACK) ? un_ra : 12'd0;
    assign sp_we = (cur == OP_LD) ? ld_we : (cur == OP_MM) ? mm_we : (cur == OP_RQ) ? rq_we : (cur == OP_SM) ? sm_we : (cur == OP_VADD) ? va_we : (cur == OP_AMAX) ? am_we : (cur == OP_UNPACK) ? un_we : 1'b0;
    assign sp_wa = (cur == OP_LD) ? ld_wa : (cur == OP_MM) ? mm_wa : (cur == OP_RQ) ? rq_wa : (cur == OP_SM) ? sm_wa : (cur == OP_VADD) ? va_wa : (cur == OP_UNPACK) ? un_wa : am_wa;
    assign sp_wd = (cur == OP_LD) ? ld_wd : (cur == OP_MM) ? mm_wd : (cur == OP_RQ) ? rq_wd : (cur == OP_SM) ? sm_wd : (cur == OP_VADD) ? va_wd : (cur == OP_UNPACK) ? un_wd : am_wd;
    wire unit_done = (cur == OP_LD && ld_done) || (cur == OP_ST && st_done) || (cur == OP_MM && mm_done) || (cur == OP_RQ && rq_done) || (cur == OP_SM && sm_done) || (cur == OP_VADD && va_done) || (cur == OP_AMAX && am_done) || (cur == OP_UNPACK && un_done);
    always @(posedge clk) begin
        if (rst) begin st <= IDLE; halted <= 1'b0; cur <= 4'd15; end
        else case (st)
            IDLE: if (start) begin pc <= 0; st <= FETCH; halted <= 1'b0; cyc_total <= 0; cyc_mm <= 0; cyc_dma <= 0; cyc_vec <= 0; n_inst <= 0; end
            FETCH: begin ins <= imem_data; st <= ISSUE; cyc_total <= cyc_total + 1; end
            ISSUE: begin
                cyc_total <= cyc_total + 1; cur <= op;
                if (op == OP_HALT) begin st <= HALT; halted <= 1'b1; cur <= 4'd15; end else st <= WAIT;
            end
            WAIT: begin
                cyc_total <= cyc_total + 1;
                if (cur == OP_MM) cyc_mm <= cyc_mm + 1; else if (cur == OP_LD || cur == OP_ST) cyc_dma <= cyc_dma + 1; else cyc_vec <= cyc_vec + 1;
                if (unit_done) begin pc <= pc + 1; n_inst <= n_inst + 1; st <= FETCH; cur <= 4'd15; end
            end
            HALT: ;
        endcase
    end
endmodule
