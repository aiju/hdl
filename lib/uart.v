/* TODO: implement rx */
`default_nettype none

module uart(
	input wire clk,
	
	output reg tx,
	input wire rx,
	
	input wire txreq,
	output reg txack,
	input wire [7:0] txdata,
	
	output reg rxreq,
	input wire rxack,
	output reg [7:0] rxdata
);

	parameter SYSHZ = 100_000_000;
	parameter BAUD = 9600;
	localparam INT = SYSHZ / BAUD;
	
	reg txact;
	reg [8:0] txbuf;
	reg [31:0] txtim;
	reg [3:0] txctr;
	
	initial begin
		txact = 1'b0;
		tx = 1'b1;
	end
	
	always @(posedge clk) begin
		txack <= 1'b0;

		if(!txact) begin
			if(txreq) begin
				txbuf <= {1'b1, txdata};
				txact <= 1'b1;
				txctr <= 4'b0;
				txtim <= INT-1;
				tx <= 1'b0;
			end
		end else
			if(txtim == 32'b0) begin
				if(txctr == 4'd8) begin
					txact <= 1'b0;
					txack <= 1'b1;
				end else begin
					tx <= txbuf[0];
					txbuf <= {1'b1, txbuf[8:1]};
					txtim <= INT-1;
					txctr <= txctr + 1;
				end
			end else
				txtim <= txtim - 1;
	end

endmodule
