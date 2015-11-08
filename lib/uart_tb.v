`default_nettype none

module uart_tb;

	reg clk;
	wire tx, rx, txack, rxreq, rxack;
	wire [7:0] rxdata;
	reg txreq;
	reg [7:0] txdata;
	
	initial clk = 0;
	always #0.5 clk = !clk;
	
	initial begin
		$dumpvars;
		$dumpfile("dump.vcd");
		txreq = 0;
		txdata = 8'h00;
		@(posedge clk) @(posedge clk) txreq <= 1;
		@(posedge clk) txreq <= 0;
		@(posedge clk && txack) txreq <= 1;
		#1000000 $finish;
	end

	uart uart0(clk, tx, rx, txreq, txack, txdata, rxreq, rxack, rxdata);

endmodule
