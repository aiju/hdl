`default_nettype none

module hjdebug #(parameter N = 1, parameter SUM = 0, parameter SIZ = 1024) (
	input wire clk,
	
	input wire regreq,
	input wire regwr,
	input wire [11:0] regaddr,
	input wire [31:0] regwdata,
	output reg regack,
	output reg regerr,
	output reg [31:0] regrdata,
	
	input wire [N-1:0] in
);

	reg [N-1:0] mem[0:SIZ-1];
	reg [15:0] wptr, rptr, tpos, rctr;
	reg running = 1'b0, avail = 1'b0, full, start, abort, start0, step, trans;
	reg step0 = 1'b0, step1 = 1'b0;
	reg [N-1:0] memrdata, memrdata0, rdshreg, in0, in1, lastwr;
	wire trigger;
	genvar i;
	
	wire memwe = running && (!full || !trigger) && (!trans || start0 || in1 != in0);
	always @(posedge clk) begin
		if(memwe || step)
			memrdata <= mem[rptr];
		if(memwe) begin
			mem[wptr] <= in0;
			lastwr <= in0;
		end
	end
	wire [N-1:0] memrdata_ = tpos == SIZ-1 ? lastwr : memrdata;

	always @(posedge clk) begin
		memrdata0 <= memrdata_;
		start0 <= start;
		in0 <= in;
		in1 <= in0;
		if(running) begin
			if(full && trigger) begin
				running <= 1'b0;
				avail <= 1'b1;
				rptr <= wptr;
			end else if(memwe) begin
				if(wptr == SIZ - 1) begin
					wptr <= 0;
					full <= 1;
				end else
					wptr <= wptr + 1;
				rptr <= rptr == SIZ - 1 ? 0 : rptr + 1;
			end
		end
		if(step0) begin
			if(rctr == 0)
				rdshreg <= memrdata;
			else
				rdshreg <= rdshreg >> 32;
			if(rctr + 32 >= N) begin
				rctr <= 0;
				rptr <= rptr == SIZ - 1 ? 0 : rptr + 1;
			end else
				rctr <= rctr + 32;
		end
		if(abort) begin
			running <= 1'b0;
			avail <= 1'b0;
		end
		if(start) begin
			running <= 1'b1;
			avail <= 1'b0;
			full <= 1'b0;
			wptr <= 0;
			rptr <= tpos >= SIZ-1 ? 0 : tpos + 1;
			rctr <= 0;
		end
	end
	
	localparam NL = (2*N+4)/5;
	wire [NL:0] daisy;
	wire [NL-1:0] lout;
	wire [2*N-1:0] tinp = {memrdata0, memrdata_};
	reg srbusy = 1'b0;
	assign trigger = !srbusy && &lout;
	
	for(i = 0; i < NL; i = i + 1) begin :trig
		CFGLUT5 #(.INIT(-1)) L(
			.CLK(clk),
			.CE(srbusy),
			.CDI(daisy[i+1]),
			.CDO(daisy[i]),
			.I0(tinp[5 * i]),
			.I1(tinp[5 * i + 1]),
			.I2(tinp[5 * i + 2]),
			.I3(tinp[5 * i + 3]),
			.I4(tinp[5 * i + 4]),
			.O6(lout[i])
		);
	end
	
	reg startsr;
	reg [31:0] sr;
	reg [4:0] ctr = 0;
	assign daisy[NL] = sr[31];
	
	always @(posedge clk) begin
		start <= 1'b0;
		abort <= 1'b0;
		step <= 1'b0;
		step0 <= step;
		step1 <= step0;
		regack <= 1'b0;
	
		if(regreq) begin
			regack <= 1'b1;
			regerr <= 1'b0;
			if(regwr)
				case(regaddr & ~3)
				0: begin
					start <= regwdata[0];
					abort <= regwdata[1];
					trans <= regwdata[8];
					tpos <= regwdata[31:16];
				end
				12: begin
					startsr <= 1'b1;
					regack <= 1'b0;
				end
				default: regerr <= 1'b1;
				endcase
			else
				case(regaddr & ~3)
				0: regrdata <= {tpos, 7'b0, trans, 4'b0, running, avail, 2'b0};
				4: regrdata <= SIZ;
				8: begin
					regack <= 1'b0;
					step <= 1'b1;
				end
				16: regrdata <= SUM;
				default: regerr <= 1'b1;
				endcase
		end
		if(step1) begin
			regrdata <= rdshreg;
			regack <= 1'b1;
		end
		if(srbusy) begin
			sr <= {sr[30:0], 1'b0};
			if(ctr == 31)
				srbusy <= 1'b0;
			ctr <= ctr + 1;
		end
		if((!srbusy || ctr == 31) && startsr) begin
			sr <= regwdata;
			srbusy <= 1'b1;
			regack <= 1'b1;
			startsr <= 1'b0;
		end
	end

endmodule
