`default_nettype none

module descrack (
	input wire clk,
	input wire run,
	input wire [63:0] start,
	input wire [63:0] goal,
	output reg [63:0] result,
	output reg busy
);

	wire [63:0] id = 64'h4141414141414141;
	
	reg [55:0] ctr;
	reg carry0, carry1, carry2;
	wire [63:0] key = {ctr[55:49], 1'b0, ctr[48:42], 1'b0, ctr[41:35], 1'b0, ctr[34:28], 1'b0, ctr[27:21], 1'b0, ctr[20:14], 1'b0, ctr[13:7], 1'b0, ctr[6:0], 1'b0};
	wire outvalid;
	wire [63:0] od;
	wire runs;
	reg runs0;
	
	sync sync0(clk, run, runs);
	
	initial begin
		busy = 0;
	end

	always @(posedge clk) begin : block
		result <= key;
		if(busy) begin
			carry0 <= ctr[13:0] == 14'h3ffe;
			carry1 <= ctr[27:0] == 28'hfff_fffe;
			carry2 <= ctr[55:0] == 42'h3ff_ffff_fffe;
			ctr[13:0] <= ctr[13:0] + 1;
			if(carry0)
				ctr[27:14] <= ctr[27:14] + 1;
			if(carry1)
				ctr[41:28] <= ctr[41:28] + 1;
			if(carry2)
				ctr[55:42] <= ctr[55:42] + 1;
		end
		if(busy && outvalid && od[63:8] == goal[63:8])
			busy <= 1'b0;
		runs0 <= runs;
		if(runs && !runs0) begin
			busy <= 1'b1;
			ctr <= {start[63:57], start[55:49], start[47:41], start[39:33], start[31:25], start[23:17], start[15:9], start[7:1]};
			carry0 <= 0;
			carry1 <= 0;
			carry2 <= 0;
		end
	end

	des des0(
		.clk(clk),
		.invalid(busy),
		.id(id),
		.key(key),
		.outvalid(outvalid),
		.od(od)
	);

endmodule
