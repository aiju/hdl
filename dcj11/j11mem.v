`default_nettype none

module j11mem(
	input wire clk,

	input wire memreq,
	input wire memwr,
	input wire [21:0] memaddr,
	input wire [15:0] memwdata,
	output reg memack,
	output reg [15:0] memrdata
);

	reg [15:0] mem[0:4095];
	
	always @(posedge clk) begin
		memack <= memreq;
		if(memreq)
			if(memwr)
				mem[memaddr] <= memwdata;
			else
				memrdata <= mem[memaddr];
	end

endmodule
