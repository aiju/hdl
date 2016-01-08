`default_nettype none

module uart11(
	input wire clk,
	input wire busrst,
	
	input wire uartreq,
	input wire [2:0] uartaddr,
	input wire uartwr,
	input wire [15:0] uartwdata,
	output reg uartack,
	output reg [15:0] uartrdata,
	
	input wire uarthostreq,
	input wire [2:0] uarthostaddr,
	input wire uarthostwr,
	input wire [31:0] uarthostwdata,
	output reg uarthostack,
	output reg [31:0] uarthostrdata
);

	wire [7:0] txrddata, txwrdata, rxrddata, rxwrdata;
	wire txfull, txempty, rxfull, rxempty;
	reg rxrden, txrden, txwren, rxwren;
	
	assign txwrdata = uartwdata[7:0];
	assign rxwrdata = uarthostwdata[7:0];
	wire [15:0] rcsr = {8'd0, !rxempty, 7'd0};
	wire [15:0] xcsr = {8'd0, !txfull, 7'd0};

	always @(posedge clk) begin
		rxrden <= 1'b0;
		txwren <= 1'b0;
		uartack <= 1'b0;

		if(uartreq) begin
			uartack <= 1'b1;
			uartrdata <= 16'bx;
			casex({uartwr, uartaddr})
			4'b000x: uartrdata <= rcsr;
			4'b001x: begin
				uartrdata <= {8'b0, rxrddata};
				rxrden <= !rxempty;
			end
			4'b010x: uartrdata <= xcsr;
			4'b111x: txwren <= 1'b1;
			endcase
		end
	end
	
	always @(posedge clk) begin
		txrden <= 1'b0;
		rxwren <= 1'b0;
		uarthostack <= 1'b0;
		
		if(uarthostreq) begin
			uarthostack <= 1'b1;
			uarthostrdata <= 32'bx;
			casex({uarthostwr, uarthostaddr})
			4'b00xx: begin
				uarthostrdata <= {!txempty, 23'd0, txrddata};
				txrden <= !txempty;
			end
			4'b01xx: uarthostrdata <= {!rxfull, 31'd0};
			4'b11xx: rxwren <= 1'b1;
			endcase
		end
	end
	
	fifo #(.WIDTH(8)) txfifo(
		.rst(busrst),
		.rdclk(clk),
		.rden(txrden),
		.rddata(txrddata),
		.wrclk(clk),
		.wren(txwren),
		.wrdata(txwrdata),
		.full(txfull),
		.empty(txempty)),
		rxfifo(
		.rst(busrst),
		.rdclk(clk),
		.rden(rxrden),
		.rddata(rxrddata),
		.wrclk(clk),
		.wren(rxwren),
		.wrdata(rxwrdata),
		.full(rxfull),
		.empty(rxempty));
		

endmodule
