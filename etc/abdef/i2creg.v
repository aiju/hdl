module i2creg(
	input wire clk,
	
	inout wire sda,
	output wire sdain,
	input wire sdaout,
	
	input wire regreq,
	output reg regack,
	input wire [31:0] regwdata,
	output reg [31:0] regrdata,
	input wire regwr,

	output reg [7:0] i2caddr,
	output reg [7:0] i2cwdata,
	output reg i2creq,
	output reg i2clast,
	input wire [7:0] i2crdata,
	input wire i2cack,
	input wire i2cerr
);

	IOBUF sdabuf(
		.T(sdaout),
		.I(1'b0),
		.O(sdain),
		.IO(sda)
	);

	always @(posedge clk) begin
		regack <= 1'b0;
		if(regreq) begin
			regack <= 1'b1;
			if(regwr) begin
				if(!i2creq) begin
					i2creq <= 1'b1;
					i2clast <= regwdata[16];
					i2caddr <= regwdata[15:8];
					i2cwdata <= regwdata[7:0];
				end
			end else
				regrdata <= {i2creq, i2cerr, i2crdata};
		end
		if(i2cack)
			i2creq <= 1'b0;
	end

endmodule
