`default_nettype none

module hjdebug #(parameter N = 1, parameter SIZ = 1024) (
	input wire clk,
	
	input wire regreq,
	input wire regwr,
	input wire [15:0] regaddr,
	input wire [31:0] regwdata,
	output reg regack,
	output reg regerr,
	output reg [31:0] regrdata,
	
	input wire [N-1:0] in
);

	reg [N-1:0] mem[0:SIZ-1];
	reg [15:0] wptr, rptr, tpos, rctr;
	reg running = 1'b0, avail = 1'b0, full, start, start0, step, trans;
	reg [N-1:0] memrdata, memrdata0, rdshreg, in0;
	genvar i;
	
	wire memwe = running && (!full || !trigger) && (!trans || start0 || in0 != in);
	always @(posedge clk) begin
		if(memwe || step)
			memrdata <= mem[rptr];
		if(memwe) begin
			mem[wptr] <= in;
			if(rptr == wptr)
				memrdata <= in;
		end
	end

	always @(posedge clk) begin
		memrdata0 <= memrdata;
		start0 <= start;
		in0 <= in;
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
	wire [NL-1:0] daisy;
	wire [NL-1:0] lout;
	wire [2*N-1:0] tinp = {memrdata0, memrdata};
	wire trigger = &lout;
	
	for(i = 0; i < (N+4)/5; i = i + 1) begin :lvl0
		CFGLUT5 L(
			.CLK(clk),
			.CE(srbusy),
			.CDI(daisy[i+1]),
			.CDO(daisy[i]),
			.IN(tinp[5 * i +: 5]),
			.OUT(lout[i])
		);
	end
	
	reg startsr;
	reg srbusy = 1'b0, srpend = 1'b0;
	reg [31:0] sr;
	reg [4:0] ctr = 0;
	always @(posedge clk) begin
		if(srbusy) begin
			sr <= {sr[30:0], 1'b0};
			if(ctr == 31) begin
				srbusy <= srpend;
				sr <= regwdata;
				srpend <= 1'b0;
			end
			ctr <= ctr + 1;
		end
		if(startsr) begin
			if(srbusy)
				srpend <= 1'b1;
			else begin
				sr <= regwdata;
				srbusy <= 1'b1;
			end
		end
	end
	assign daisy[NL-1] = sr[31];
	reg step0 = 1'b0, step1 = 1'b0;
	
	always @(posedge clk) begin
		start <= 1'b0;
		startsr <= 1'b0;
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
					trans <= regwdata[1];
					tpos <= regwdata[31:16];
				end
				12: begin
					startsr <= 1'b1;
					if(srbusy)
						regack <= 1'b0;
				end
				default: regerr <= 1'b1;
				endcase
			else
				case(regaddr & ~3)
				0: regrdata <= {31'b0, avail};
				4: regrdata <= N;
				8: begin
					regack <= 1'b0;
					step <= 1'b1;
				end
				default: regerr <= 1'b1;
				endcase
		end
		if(step1) begin
			regrdata <= rdshreg;
			regack <= 1'b1;
		end	
	end

endmodule
