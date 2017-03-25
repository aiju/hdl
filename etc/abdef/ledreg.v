`default_nettype none

module ledreg(
	input wire clk,
	
	input wire regreq,
	output reg regack,
	input wire regwr,
	input wire [31:0] regwdata,
	output wire [31:0] regrdata,
	
	output reg [5:0] leds
);

	assign regrdata = {26'b0, leds};
	always @(posedge clk) begin
		regack <= 1'b0;
		if(regreq) begin
			regack <= 1'b1;
			if(regwr)
				leds <= regwdata[5:0];
		end
	end

endmodule
