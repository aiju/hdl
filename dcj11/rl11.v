`default_nettype none

module rl11(
	input wire clk,
	input wire busrst,
	
	input wire rlreq,
	input wire [2:0] rladdr,
	input wire rlwr,
	input wire [15:0] rlwdata,
	output reg rlack,
	output reg [15:0] rlrdata,
	
	output reg rlirq,
	
	output reg rlmemreq,
	output reg rlmemwr,
	output reg [31:0] rlmemaddr,
	output wire [15:0] rlmemwdata,
	input wire rlmemack,
	input wire [15:0] rlmemrdata,
	
	output reg rldmareq,
	output wire rldmawr,
	output wire [17:0] rldmaaddr,
	output wire [15:0] rldmawdata,
	input wire rldmaack,
	input wire [15:0] rldmardata
);

	reg csrdrdy = 1'b1;
	reg [2:0] csrf = 3'b00;
	reg [1:0] csrba = 2'b00;
	reg csrie = 1'b0;
	reg csrcrdy = 1'b1, csrcrdy0 = 1'b1;
	reg [1:0] csrds = 2'b00;
	reg [3:0] csre = 4'b0000;
	reg csrde = 1'b0;
	wire csrerr = csre != 0;
	
	reg [15:0] drstat = 'o235;
	
	reg [15:0] ba;
	reg [15:0] da;
	reg [15:0] mp;
	
	always @(posedge clk) begin
		csrcrdy0 <= csrcrdy;
	end
	
	assign rldmaaddr = {csrba, ba};
	assign rldmawr = !rlmemwr;
	assign rldmawdata = rlmemrdata;
	assign rlmemwdata = rldmardata;
	
	always @(posedge clk) begin : regs
		reg csrgo, csrie_;
	
		rlirq <= 1'b0;
		csrgo = 1'b0;
		csrie_ = csrie;
		rlmemreq <= 1'b0;
		rldmareq <= 1'b0;
		rlack <= rlreq;
		if(rlreq) begin
			casez({rladdr, rlwr})
			4'b00z0:
				rlrdata <= {csrerr, csrde, csrds, csrcrdy, csrie, csrba, csrf, csrdrdy};
			4'b00z1: begin
				{csrds, csrcrdy, csrie, csrba, csrf} <= rlwdata[9:1];
				csrgo = csrcrdy && !rlwdata[7];
				csrie_ = rlwdata[6];
			end
			4'b01z0:
				rlrdata <= ba;
			4'b01z1:
				ba <= rlwdata;
			4'b10z0:
				rlrdata <= da;
			4'b10z1:
				da <= rlwdata;
			4'b11z0:
				rlrdata <= mp;
			4'b11z1:
				mp <= rlwdata;
			endcase
		end

		if(csrgo) begin
			case(rlwdata[3:1])
			2: begin
				csrcrdy <= 1'b1;
				if(csrie_) rlirq <= 1'b1;
				if(da[3]) begin
					drstat <= drstat & 'hff;
					mp <= drstat & 'hff;
				end else
					mp <= drstat;	
			end
			5: begin
				if(mp == 0) begin
					csrcrdy <= 1'b1;
					if(csrie_) rlirq <= 1'b1;
				end else
					rldmareq <= 1'b1;
				mp <= mp + 1;
				rlmemaddr <= 256 * (da[5:0] + 40 * da[15:6]);
				rlmemwr <= 1'b1;
			end				
			6: begin
				if(mp == 0) begin
					csrcrdy <= 1'b1;
					if(csrie_) rlirq <= 1'b1;
				end else
					rlmemreq <= 1'b1;
				mp <= mp + 1;
				rlmemaddr <= 256 * (da[5:0] + 40 * da[15:6]);
				rlmemwr <= 1'b0;
			end
			default: begin
				csrcrdy <= 1'b1;
				if(csrie_) rlirq <= 1'b1;
			end
			endcase
		end
		if(rlmemack) begin
			if(rlmemwr)
				if(mp != 0) begin
					rldmareq <= 1'b1;
					mp <= mp + 1;
				end else begin
					csrcrdy <= 1'b1;
					if(csrie) rlirq <= 1'b1;
				end
			else
				rldmareq <= 1'b1;
			rlmemaddr <= rlmemaddr + 2;
		end
		if(rldmaack) begin
			if(!rlmemwr)
				if(mp != 0) begin
					rlmemreq <= 1'b1;
					mp <= mp + 1;
				end else begin
					csrcrdy <= 1'b1;
					if(csrie) rlirq <= 1'b1;
				end
			else
				rlmemreq <= 1'b1;
			ba <= ba + 2;
		end
	end

endmodule
