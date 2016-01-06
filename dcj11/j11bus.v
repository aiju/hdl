`default_nettype none

module j11bus(
	input wire clk,

	input wire busreq,
	input wire buswr,
	input wire busgp,
	input wire [21:0] busaddr,
	input wire [15:0] buswdata,
	output reg busack,
	output reg [15:0] busrdata,
	
	output reg memreq,
	output wire memwr,
	output wire [21:0] memaddr,
	output wire [15:0] memwdata,
	input wire memack,
	input wire [15:0] memrdata,
	
	output reg j11init,
	output reg j11halt,
	output reg j11parity,
	output reg j11event,
	output reg [3:0] j11irq,
	output reg j11pwrf,
	output reg j11fpe,
	
	input wire regreq,
	input wire [31:0] regwdata,
	output reg regack
);

	localparam powerup = {1'b0, 2'b01, 1'b1};

	initial begin
		j11init = 1'b0;
		j11halt = 1'b1;
		j11parity = 1'b1;
		j11event = 1'b1;
		j11irq = 4'b1111;
		j11pwrf = 1'b1;
		j11fpe = 1'b1;
	end
	
	assign memwr = buswr;
	assign memaddr = busaddr;
	assign memwdata = buswdata;
	
	always @(posedge clk) begin
		busack <= 1'b0;
		memreq <= 1'b0;
		if(busreq)
			if(busgp) begin
				busack <= 1'b1;
				busrdata <= 16'bx;
				case({buswr, busaddr[7:0]})
				{1'b1, 8'o0}: busrdata <= powerup;
				endcase
			end else
				memreq <= 1'b1;
		if(memack) begin
			busack <= 1'b1;
			busrdata <= memrdata;
		end
	end
	
	
	always @(posedge clk) begin
		regack <= regreq;
		if(regreq)
			j11init <= regwdata[0];
	end

endmodule
