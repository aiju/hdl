`default_nettype none

module hjdebug_tb;

	reg clk = 1'b1;
	
	reg regreq, regwr;
	reg [11:0] regaddr;
	reg [31:0] regwdata;
	wire regack;
	wire regerr;
	wire [31:0] regrdata;
	
	wire [7:0] in;
	
	hjdebug #(.N(8), .SIZ(16)) hjdebug0(clk, regreq, regwr, regaddr, regwdata, regack, regerr, regrdata, in);
	
	always #0.5 clk = !clk;
	
	initial regreq = 1'b0;
	
	task regread(input [11:0] a, output [31:0] v);
	begin
		regreq <= 1'b1;
		regwr <= 1'b0;
		regaddr <= a;
		regwdata <= 32'bx;
		@(posedge clk) regreq <= 1'b0;
		while(!regack)
			@(posedge clk);
		v = regrdata;
	end
	endtask
	
	task regwrite(input [11:0] a, input [31:0] v);
	begin
		regreq <= 1'b1;
		regwr <= 1'b1;
		regaddr <= a;
		regwdata <= v;
		@(posedge clk) regreq <= 1'b0;
		while(!regack)
			@(posedge clk);	
	end
	endtask
	
	initial begin : main
		reg [31:0] v, i;

		@(posedge clk);
		regwrite(12, -1);
		regwrite(12, -1);
		regwrite(12, -1);
		regwrite(12, -1);
		regwrite(0, {16'd8, 14'd0, 1'b0, 1'b1});
		v = 1;
		while(v[0] == 1)
			regread(0, v);
		for(i = 0; i < 16; i = i + 1) begin
			regread(8, v);
			$display("%d %h", i, v);
		end
	end
	
	initial #10000 $finish;
	initial begin
		$dumpfile("dump.vcd");
		$dumpvars;
	end
	
	reg [31:0] v = 0;
	always @(posedge clk)
		v = v + 1;
	assign in = v[10:2];

endmodule

`ifdef SIMULATION

module CFGLUT5 #(parameter INIT = 32'd0) (
	input wire CLK,
	input wire CE,
	input wire CDI,
	output reg CDO,
	input wire I0,
	input wire I1,
	input wire I2,
	input wire I3,
	input wire I4,
	output wire O5,
	output wire O6
);

	reg [31:0] sr = INIT;

	assign O6 = sr[{I4, I3, I2, I1, I0}];
	assign O5 = sr[{I3, I2, I1, I0}];
	
	always @(posedge CLK)
		if(CE)
			{CDO, sr} <= {sr, CDI};

endmodule

`endif
