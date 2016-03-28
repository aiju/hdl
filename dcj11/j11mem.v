`default_nettype none

module j11mem(
	input wire clk,

	input wire memreq,
	input wire memwr,
	input wire [21:0] memaddr,
	input wire [15:0] memwdata,
	input wire [1:0] memwstrb,
	output reg memack,
	output reg [15:0] memrdata,
	output reg memerr,
	
	output reg dmemreq,
	output wire dmemwr,
	output reg [21:0] dmemaddr,
	output wire [15:0] dmemwdata,
	output wire [1:0] dmemwstrb,
	input wire dmemack,
	input wire [15:0] dmemrdata,
	input wire dmemerr,

	input wire rldmareq,
	input wire rldmawr,
	input wire [17:0] rldmaaddr,
	input wire [15:0] rldmawdata,
	output reg rldmaack,
	output reg [15:0] rldmardata,
	output reg rldmaerr,
		
	output reg uartreq,
	output reg [2:0] uartaddr,
	output wire uartwr,
	output wire [15:0] uartwdata,
	input wire uartack,
	input wire [15:0] uartrdata,
	
	output reg rlreq,
	output reg [2:0] rladdr,
	output wire rlwr,
	output wire [15:0] rlwdata,
	input wire rlack,
	input wire [15:0] rlrdata,

	output reg kwreq,
	output wire kwwr,
	output wire [15:0] kwwdata,
	input wire kwack,
	input wire [15:0] kwrdata,
	
	input wire mapen	
);

	localparam IDLE = 0;
	localparam CPU = 1;
	localparam RL = 2;
	
	reg unireq, unimap, uniack, uniwr, unierr;
	reg [15:0] uniwdata;
	reg [21:0] uniaddr;
	reg [15:0] unirdata;
	reg [1:0] uniwstrb;

	reg mempend, rlpend;

	reg [1:0] state;
	initial state = IDLE;
	always @(posedge clk) begin
		if(memreq) mempend <= 1'b1;
		if(rldmareq) rlpend <= 1'b1;
		unireq <= 1'b0;
		memack <= 1'b0;
		rldmaack <= 1'b0;
		case(state)
		IDLE:
			if(memreq || mempend) begin
				unireq <= 1'b1;
				uniaddr <= memaddr;
				unimap <= memaddr >= 22'o17000000;
				uniwdata <= memwdata;
				uniwr <= memwr;
				uniwstrb <= memwstrb;
				mempend <= 1'b0;
				state <= CPU;
			end else if(rldmareq || rlpend) begin
				unireq <= 1'b1;
				uniaddr <= {4'bx, rldmaaddr};
				uniwdata <= rldmawdata;
				uniwr <= rldmawr;
				unimap <= 1'b1;
				uniwstrb <= 2'b11;
				rlpend <= 1'b0;
				state <= RL;
			end
		CPU:
			if(uniack) begin
				memack <= 1'b1;
				memrdata <= unirdata;
				memerr <= unierr;
				state <= IDLE;
			end
		RL:
			if(uniack) begin
				rldmaack <= 1'b1;
				rldmardata <= unirdata;
				rldmaerr <= unierr;
				state <= IDLE;
			end
		endcase
	end
	
	reg [21:0] unibase[0:31];
	initial begin : init
		integer i;
		
		for(i = 0; i < 32; i = i + 1)
			unibase[i] = i << 13;
	end
	
	reg [21:0] unimapped;
	
	always @* begin
		if(!unimap)
			unimapped = uniaddr;
		else if(&uniaddr[17:13])
			unimapped = {9'b111111111, uniaddr[12:0]};
		else if(mapen)
			unimapped = unibase[uniaddr[17:13]] + uniaddr[12:0];
		else
			unimapped = {4'b0, uniaddr[17:0]};
	end
	
	assign dmemwr = uniwr;
	assign dmemwdata = uniwdata;
	assign dmemwstrb = uniwstrb;
	assign uartwr = uniwr;
	assign uartwdata = uniwdata;
	assign rlwr = uniwr;
	assign rlwdata = uniwdata;
	assign kwwr = uniwr;
	assign kwwdata = uniwdata;
	
	reg mapreq, mapack;
	reg [6:0] mapaddr;
	reg [15:0] maprdata;
	
	always @(posedge clk) begin
		dmemreq <= 1'b0;
		mapreq <= 1'b0;
		uartreq <= 1'b0;
		rlreq <= 1'b0;
		uniack <= 1'b0;
		kwreq <= 1'b0;
		if(unireq) begin
			unierr <= 1'b0;
			if(!(&unimapped[21:13])) begin
				dmemreq <= 1'b1;
				dmemaddr <= unimapped;
			end else
				casez(unimapped[12:0])
				13'b1_111_111_100_zzz: begin
					uniack <= 1'b1;
					unirdata <= 0;
				end
				13'b1_000_01z_zzz_zzz: begin
					mapreq <= 1'b1;
					mapaddr <= unimapped;
				end
				13'b1_111_101_110_zzz: begin
					uartreq <= 1'b1;
					uartaddr <= unimapped;
				end
				13'b1_100_100_000_zzz: begin
					rlreq <= 1'b1;
					rladdr <= unimapped;
				end
				13'b1_111_101_100_110:
					kwreq <= 1'b1;
				default: begin
					uniack <= 1'b1;
					unierr <= 1'b1;
				end
				endcase
		end
		if(dmemack) begin
			uniack <= 1'b1;
			unirdata <= dmemrdata;
			unierr <= dmemerr;
		end
		if(uartack) begin
			uniack <= 1'b1;
			unirdata <= uartrdata;
		end
		if(rlack) begin
			uniack <= 1'b1;
			unirdata <= rlrdata;
		end
		if(mapack) begin
			uniack <= 1'b1;
			unirdata <= maprdata;
		end
		if(kwack) begin
			uniack <= 1'b1;
			unirdata <= kwrdata;
		end
	end
	
	always @(posedge clk) begin
		mapack <= 1'b0;
		if(mapreq) begin
			mapack <= 1'b1;
			if(uniwr)
				if(mapaddr[1])
					unibase[mapaddr[6:2]][22:16] <= uniwdata;
				else
					unibase[mapaddr[6:2]][15:0] <= uniwdata & ~1;
			else
				if(mapaddr[1])
					maprdata <= unibase[mapaddr[6:2]][21:16];
				else
					maprdata <= unibase[mapaddr[6:2]][15:0];
		end
	end

endmodule
