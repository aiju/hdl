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
	
	reg [63:0] key;
	wire outvalid;
	wire [63:0] od;
	wire run0;
	
	sync sync0(clk, run, run0);
	
	initial begin
		busy = 0;
	end

	always @(posedge clk) begin
		result <= key;
		if(busy)
			{key[63:57], key[55:49], key[47:41], key[39:33], key[31:25], key[23:17], key[15:9], key[7:1]} <= {key[63:57], key[55:49], key[47:41], key[39:33], key[31:25], key[23:17], key[15:9], key[7:1]} + 1;
		if(busy && outvalid && od[63:8] == goal[63:8])
			busy <= 1'b0;
		if(run && !run0) begin
			busy <= 1'b1;
			key <= start;
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
