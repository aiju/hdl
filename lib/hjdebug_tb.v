`default_nettype none

module hjdebug_tb;

	reg clk = 1'b1;
	
	reg regreq, regwr;
	reg [11:0] regaddr;
	reg [31:0] regwdata;
	wire regack;
	wire regerr;
	wire [31:0] regrdata;
	
	parameter N = 8;
	parameter SIZ = 16;
	parameter DIV = 2;
	
	reg [N-1:0] in = 0;
	reg [DIV-1:0] div = 0;
	always @(posedge clk)
		{in, div} <= {in, div} + 1;
	
	hjdebug #(.N(N), .SIZ(SIZ)) hjdebug0(clk, regreq, regwr, regaddr, regwdata, regack, regerr, regrdata, in);
	
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
	
	task trig5(input [4:0] a);
		integer i, j;
		reg [31:0] sr;
	begin
		sr = -1;
		for(i = 0; i < 5; i = i + 1)
			for(j = 0; j < 32; j = j + 1)
				if(j[i] === !a[i])
					sr[j] = 1'b0;
		regwrite(12, sr);
	end
	endtask
	
	task trigv(input [N-1:0] a);
		integer i, j;
		reg [4:0] r;
	begin
		j = 0;
		for(i = 0; i < N; i = i + 1) begin
			r[j] = a[i];
			j = j + 1;
			if(j == 5) begin
				trig5(r);
				j = 0;
			end
		end
		for(i = 0; i < N; i = i + 1) begin
			r[j] = 1'bx;
			j = j + 1;
			if(j == 5) begin
				trig5(r);
				j = 0;
			end
		end
		if(j != 0) begin
			for(j = j; j < 5; j = j + 1)
				r[j] = 1'bx;
			trig5(r);
		end
	end
	endtask
	
	reg [N-1:0] pat[0:SIZ-1];
	reg fail;
	
	task filltest(input [N-1:0] tval, input [15:0] tpoint, input trans);
		integer i;
	begin
		for(i = 0; i < SIZ; i = i + 1)
			pat[i] = tval + (i - tpoint >>> (trans ? 0 : DIV));
	end
	endtask
	
	
	task trial(input [N-1:0] tval, input [15:0] tpoint, input trans);
		integer i;
		reg [31:0] v;
	begin
		regwrite(0, 1<<1);
		trigv(tval);
		regwrite(0, {tpoint, 7'd0, trans, 7'b0, 1'b1});
		v = 1;
		while(v[2] == 0)
			regread(0, v);
		filltest(tval, tpoint, trans);
		for(i = 0; i < SIZ; i = i + 1) begin
			regread(8, v);
			if(v[N-1:0] !== pat[i]) begin
				fail = 1'b1;
				$display("FAIL (trigger %2d @ %2d, trans %0d): %2d: %h !== %h", tval, tpoint, trans, i, v[N-1:0], pat[i]);
			end
		end
	end
	endtask
	
	initial begin
		@(posedge clk);
		fail = 1'b0;
		trial(5, 0, 0);
		trial(5, SIZ/2, 0);
		trial(5, SIZ-1, 0);
		trial(5, 0, 1);
		trial(5, SIZ/2, 1);
		trial(5, SIZ-1, 1);
		if(!fail)
			$display("PASS");
		$finish;
	end
	
	initial begin
		$dumpfile("dump.vcd");
		$dumpvars;
	end

endmodule

`ifdef SIMULATION

module CFGLUT5 #(parameter INIT = 32'd0) (
	input wire CLK,
	input wire CE,
	input wire CDI,
	output wire CDO,
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
	assign CDO = sr[31];
	
	always @(posedge CLK)
		if(CE)
			sr <= {sr[30:0], CDI};

endmodule

`endif
