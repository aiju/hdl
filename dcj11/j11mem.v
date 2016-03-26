`default_nettype none

module j11mem(
	input wire clk,

	input wire memreq,
	input wire memwr,
	input wire [21:0] memaddr,
	input wire [15:0] memwdata,
	output wire memack,
	output wire [15:0] memrdata,
	
	output wire dmemreq,
	output wire dmemwr,
	output wire [21:0] dmemaddr,
	output wire [15:0] dmemwdata,
	input wire dmemack,
	input wire [15:0] dmemrdata	
);

	assign dmemreq = memreq;
	assign dmemwr = memwr;
	assign dmemaddr = memaddr;
	assign dmemwdata = memwdata;
	assign memack = dmemack;
	assign memrdata = dmemrdata;

endmodule
