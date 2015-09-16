module descrack_tb;

	reg clk, run;
	wire [63:0] result;
	wire busy;
	reg busy0;
	
	initial clk = 0;
	always #500 clk = !clk;
	
	wire [63:0] start = 0;
	wire [63:0] key = 64'h200;
	wire [63:0] goal = 64'h6c38184c57d157ee;
	initial begin
		$dumpfile("build/dump.vcd");
		$dumpvars;
		run = 0;
		@(posedge clk) @(posedge clk) @(posedge clk) run = 1;
	end
	
	always @(posedge clk) begin
		busy0 <= busy;
		if(!busy && busy0) begin
			if(result >= key - 32)
				$display("TEST PASSED");
			else
				$display("TEST FAILED");
			$finish;
		end
	end
	
	initial #200000 begin
		$display("TEST FAILED - TIMEOUT");
		$finish;
	end
	
	descrack descrack0(
		.clk(clk),
		.run(run),
		.start(start),
		.goal(goal),
		.result(result),
		.busy(busy)
	);
endmodule
