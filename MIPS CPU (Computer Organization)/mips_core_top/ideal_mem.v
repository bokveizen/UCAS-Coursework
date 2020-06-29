/* =========================================
* Ideal Memory Module for MIPS CPU Core
* Synchronize write (clock enable)
* Asynchronize read (do not use clock signal)
*
* Author: Yisong Chang (changyisong@ict.ac.cn)
* Date: 31/05/2016
* Version: v0.0.1
*===========================================
*/

`timescale 10 ns / 1 ns

module ideal_mem #(
	parameter ADDR_WIDTH = 14,
	parameter MEM_WIDTH = 2 ** (ADDR_WIDTH - 2)
	) (
	input			clk,			//source clock of the MIPS CPU Evaluation Module

	input [ADDR_WIDTH - 3:0]	Waddr,			//Memory write port address
	input [ADDR_WIDTH - 3:0]	Raddr1,			//Read port 1 address
	input [ADDR_WIDTH - 3:0]	Raddr2,			//Read port 2 address

	input			Wren,			//write enable
	input			Rden1,			//port 1 read enable
	input			Rden2,			//port 2 read enable

	input [31:0]	Wdata,			//Memory write data
	output [31:0]	Rdata1,			//Memory read data 1
	output [31:0]	Rdata2			//Memory read data 2
);

reg [31:0]	mem [MEM_WIDTH - 1:0];

`define ADDIU(rt, rs, imm) {6'b001001, rs, rt, imm}
`define LW(rt, base, off) {6'b100011, base, rt, off}
`define SW(rt, base, off) {6'b101011, base, rt, off}
`define BNE(rs, rt, off) {6'b000101, rs, rt, off}

  initial begin
	 // fill memory region [100, 200) with [0, 100)
	 mem[0] = `ADDIU(5'd1, 5'd0, 16'd100);
	 mem[1] = `ADDIU(5'd2, 5'd0, 16'd0);
	 mem[2] = `SW(5'd2, 5'd2, 16'd100);
	 mem[3] = `ADDIU(5'd2, 5'd2, 16'd4);
	 mem[4] = `BNE(5'd1, 5'd2, 16'hfffd);
	 
	 // copy memory region [100, 200) to memory region [200, 300)
	 mem[5] = `ADDIU(5'd2, 5'd0, 16'd0);
	 mem[6] = `LW(5'd3, 5'd2, 16'd100);
	 mem[7] = `SW(5'd3, 5'd2, 16'd200);
	 mem[8] = `ADDIU(5'd2, 5'd2, 16'd4);
	 mem[9] = `BNE(5'd1, 5'd2, 16'hfffc);
	 
	 mem[10] = `BNE(5'd1, 5'd0, 16'hffff);
  end

always @ (posedge clk)
begin
	if (Wren)
		mem[Waddr] <= Wdata;
end

assign Rdata1 = {32{Rden1}} & mem[Raddr1];
assign Rdata2 = {32{Rden2}} & mem[Raddr2];

endmodule
