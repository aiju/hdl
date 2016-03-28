`default_nettype none

module kw11(
	input wire clk,
	input wire busrst,
	
	input wire kwreq,
	input wire kwwr,
	input wire [15:0] kwwdata,
	output reg kwack,
	output wire [15:0] kwrdata,
	
	output reg kwirq
);

	parameter MHZ = 100_000_000;
	parameter HZ = 60;
	localparam [31:0] MAX = MHZ / HZ;

	reg div;
	reg [31:0] ctr = 0;

	reg mon, mon0, ie;
	assign kwrdata = {8'b0, mon, ie, 6'b0};

	always @(posedge clk) begin
		if(ctr == MAX - 1) begin
			mon <= 1'b1;
			ctr <= 0;
		end else
			ctr <= ctr + 1;
		kwack <= kwreq;
		if(kwreq && kwwr) begin
			mon <= kwwdata[7];
			ie <= kwwdata[6];
		end
		mon0 <= mon;
		if(busrst) begin
			mon <= 1'b1;
			mon0 <= 1'b1;
			ie <= 1'b0;
		end
		kwirq <= mon && !mon0 && ie;
	end

endmodule
