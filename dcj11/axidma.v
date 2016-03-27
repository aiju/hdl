`default_nettype none

module axidma(
	input wire clk,

	output wire axiaclk,
	output reg [31:0] axiaraddr,
	output wire [1:0] axiarburst,
	output wire [3:0] axiarcache,
	input wire axiaresetn,
	output reg [5:0] axiarid,
	output wire [3:0] axiarlen,
	output wire [1:0] axiarlock,
	output wire [2:0] axiarprot,
	output wire [3:0] axiarqos,
	input wire axiarready,
	output wire [1:0] axiarsize,
	output reg axiarvalid,
	output reg [31:0] axiawaddr,
	output wire [1:0] axiawburst,
	output wire [3:0] axiawcache,
	output reg [5:0] axiawid,
	output wire [3:0] axiawlen,
	output wire [1:0] axiawlock,
	output wire [2:0] axiawprot,
	output wire [3:0] axiawqos,
	input wire axiawready,
	output wire [1:0] axiawsize,
	output reg axiawvalid,
	input wire [5:0] axibid,
	output reg axibready,
	input wire [1:0] axibresp,
	input wire axibvalid,
	input wire [63:0] axirdata,
	input wire [5:0] axirid,
	input wire axirlast,
	output reg axirready,
	input wire [1:0] axirresp,
	input wire axirvalid,
	output reg [63:0] axiwdata,
	output reg [5:0] axiwid,
	output reg axiwlast,
	input wire axiwready,
	output reg [7:0] axiwstrb,
	output reg axiwvalid,
	
	input wire dmemreq,
	input wire dmemwr,
	input wire [21:0] dmemaddr,
	input wire [15:0] dmemwdata,
	input wire [1:0] dmemwstrb,
	output reg dmemack,
	output reg [15:0] dmemrdata,
	
	input wire rlmemreq,
	input wire rlmemwr,
	input wire [31:0] rlmemaddr,
	input wire [15:0] rlmemwdata,
	output reg rlmemack,
	output reg [15:0] rlmemrdata,
	
	input wire regreq,
	input wire [3:0] regaddr,
	input wire [31:0] regwdata,
	output reg regack
);

	assign axiaclk = clk;
	assign axiarburst = 2'b01;
	assign axiarcache = 4'b0010;
	assign axiarlen = 0;
	assign axiarlock = 2'b00;
	assign axiarprot = 3'b000;
	assign axiarqos = 4'b0000;
	assign axiarsize = 2'b10;
	assign axiawburst = 2'b01;
	assign axiawcache = 4'b0000;
	assign axiawlen = 0;
	assign axiawlock = 2'b00;
	assign axiawprot = 3'b000;
	assign axiawqos = 4'b0000;
	assign axiawsize = 2'b10;
	
	reg [31:0] doff, roff, dlen, rlen;
	wire dok = dmemaddr < dlen;
	wire rok = rlmemaddr < rlen;
	reg drpend, dwpend, ddpend, rrpend, rwpend, rdpend;
	wire drreq = dmemreq && !dmemwr && dok;
	wire dwreq = dmemreq && dmemwr && dok;
	wire rrreq = rlmemreq && !rlmemwr && rok;
	wire rwreq = rlmemreq && rlmemwr && rok;
	reg [1:0] drmod, rrmod;
	wire [1:0] wmod = (ddpend || dwreq ? doff + dmemaddr : roff + rlmemaddr) >> 1;
	wire [1:0] wstrb0 = ddpend || dwreq ? dmemwstrb : 2'b11;
	wire [7:0] wstrb = {{2{wmod == 2'b11}} & wstrb0, {2{wmod == 2'b10}} & wstrb0, {2{wmod == 2'b01}} & wstrb0, {2{wmod == 2'b00}} & wstrb0};
	
	initial begin
		axiarvalid = 1'b0;
		axirready = 1'b1;
		axiawvalid = 1'b0;
		axiwvalid = 1'b0;
		axibready = 1'b1;
	end
	
	always @(posedge clk) begin
		if(axiarvalid && axiarready) axiarvalid <= 1'b0;
		if(axiawvalid && axiawready) axiawvalid <= 1'b0;
		if(axiwvalid && axiwready) axiwvalid <= 1'b0;
		if(drreq) drpend <= 1'b1;
		if(dwreq) begin
			dwpend <= 1'b1;
			ddpend <= 1'b1;
		end
		if(rrreq) rrpend <= 1'b1;
		if(rwreq) begin
			rwpend <= 1'b1;
			rdpend <= 1'b1;
		end
		if(!axiarvalid || axiarready) begin
			if(drpend || drreq) begin
				axiaraddr <= doff + dmemaddr;
				drmod <= (doff + dmemaddr) >> 1;
				axiarvalid <= 1'b1;
				axiarid <= 0;
				drpend <= 1'b0;
			end else if(rrpend || rrreq) begin
				axiaraddr <= roff + rlmemaddr;
				rrmod <= (roff + rlmemaddr) >> 1;
				axiarvalid <= 1'b1;
				axiarid <= 1;
				rrpend <= 1'b0;
			end
		end
		if(!axiawvalid || axiawready) begin
			if(dwpend || dwreq) begin
				axiawaddr <= doff + dmemaddr;
				axiawvalid <= 1'b1;
				axiawid <= 0;
				dwpend <= 1'b0;
			end else if(rwpend || rwreq) begin
				axiawaddr <= roff + rlmemaddr;
				axiawvalid <= 1'b1;
				axiawid <= 1;
				rwpend <= 1'b0;
			end
		end
		if(!axiwvalid || axiwready) begin
			if(ddpend || 0 && dwreq) begin
				axiwdata <= {dmemwdata, dmemwdata, dmemwdata, dmemwdata};
				axiwstrb <= wstrb;
				axiwlast <= 1'b1;
				axiwvalid <= 1'b1;
				axiwid <= 0;
				ddpend <= 1'b0;
			end else if(rdpend || rwreq) begin
				axiwdata <= {rlmemwdata, rlmemwdata, rlmemwdata, rlmemwdata};
				axiwstrb <= wstrb;
				axiwlast <= 1'b1;
				axiwvalid <= 1'b1;
				axiwid <= 1;
				rdpend <= 1'b0;
			end
		end
	end
	
	always @(posedge clk) begin
		dmemack <= 1'b0;
		rlmemack <= 1'b0;
		if(axirvalid && axirready) begin
			if(!axirid[0]) begin
				dmemack <= 1'b1;
				dmemrdata <= axirdata[16 * drmod +: 16];
			end else begin
				rlmemack <= 1'b1;
				rlmemrdata <= axirdata[16 * rrmod +: 16];
			end
		end
		if(axibvalid && axibready) begin
			if(!axibid[0])
				dmemack <= 1'b1;
			else
				rlmemack <= 1'b1;
		end
		if(dmemreq && !dok) dmemack <= 1'b1;
		if(rlmemreq && !rok) rlmemack <= 1'b1;
	end
	
	always @(posedge clk) begin
		regack <= regreq;
		if(regreq)
			case(regaddr[3:2])
			2'b00: doff <= regwdata;
			2'b01: dlen <= regwdata;
			2'b10: roff <= regwdata;
			2'b11: rlen <= regwdata;
			endcase
	end

endmodule
