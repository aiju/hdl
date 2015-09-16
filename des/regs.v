`default_nettype none

module regs #(parameter N = 1) (
	input wire clk,
	
	input wire [31:0] armaddr,
	output reg [31:0] armrdata,
	input wire [31:0] armwdata,
	input wire armwr,
	input wire armreq,
	output reg armack,
	input wire [3:0] armwstrb,
	output reg armerr,
	
	output reg [63:0] start,
	output reg [63:0] goal,
	output reg [N-1:0] run,
	input wire [N-1:0] busy,
	input wire [64*N-1:0] res
);

	reg armreq0;
	
	always @(posedge clk) begin
		armack <= 0;
		armreq0 <= armreq;
		if(armreq && !armreq0) begin
			armack <= 1;
			armerr <= 0;
			if(armwr)
				case(armaddr[8:0] & -4)
				'h00: run <= armwdata[N-1:0];
				'h10: start[63:32] <= armwdata & 32'hfefefefe;
				'h14: start[31:0] <= armwdata & 32'hfefefefe;
				'h18: goal[63:32] <= armwdata;
				'h1c: goal[31:0] <= armwdata;
				default: armerr <= 1;
				endcase
			else
				casez(armaddr[8:0] & -4)
				'h00: armrdata <= run;
				'h04: armrdata <= busy;
				'h0C: armrdata <= N;
				'h10: armrdata <= start[63:32];
				'h14: armrdata <= start[31:0];
				'h18: armrdata <= goal[63:32];
				'h1c: armrdata <= goal[31:0];
				'b1_zzzz_zzzz: armrdata <= armaddr[2] ? res[armaddr[7:3] * 64 +: 32] : res[armaddr[7:3] * 64 + 32 +: 32];
				default: begin
					armrdata <= 32'bx;
					armerr <= 1;
				end
				endcase
		end
	end

endmodule
