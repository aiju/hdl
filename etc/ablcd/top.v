`default_nettype none

module top(
	output wire tx
);

	wire [3:0] fclkclk;
	wire clk = fclkclk[0];
	
	wire gp_arready, gp_arvalid, gp_awready, gp_awvalid, gp_bready, gp_rready, gp_wlast, gp_wvalid;
	wire gp_bvalid, gp_rvalid, gp_wready, gp_rlast;
	wire [31:0] gp_araddr, gp_awaddr, gp_ardata, gp_wdata, gp_rdata;
	wire [11:0] gp_arid, gp_awid, gp_wid, gp_bid, gp_rid;
	wire [1:0] gp_arburst, gp_awburst, gp_arlock, gp_awlock, gp_rresp, gp_bresp;
	wire [2:0] gp_arsize, gp_arprot, gp_awsize, gp_awprot;
	wire [3:0] gp_arcache, gp_awcache, gp_arlen, gp_awlen, gp_arqos, gp_awqos, gp_wstrb;
	
	wire [31:0] armaddr, armrdata, armwdata;
	wire armwr, armreq, armack, armerr;
	wire [3:0] armwstrb;
	
	wire rx;
	wire txreq;
	wire txack, rxreq, rxack;
	wire [7:0] txdata, rxdata;

	axi3 axi3i(
		clk,
		1'b1,
		gp_arvalid, gp_awvalid, gp_bready, gp_rready, gp_wlast, gp_wvalid, gp_arid, gp_awid, gp_wid, gp_arburst,
		gp_arlock, gp_arsize, gp_awburst, gp_awlock, gp_awsize, gp_arprot, gp_awprot, gp_araddr, gp_awaddr, gp_wdata,
		gp_arcache, gp_arlen, gp_arqos, gp_awcache, gp_awlen, gp_awqos, gp_wstrb, gp_arready, gp_awready, gp_bvalid,
		gp_rlast, gp_rvalid, gp_wready, gp_bid, gp_rid, gp_bresp, gp_rresp, gp_rdata,
	
		armaddr, armrdata, armwdata, armwr, armreq, armack, armwstrb, armerr
	);
	
	assign txdata = armwdata[7:0];
	assign txreq = armwr && armreq;
	assign armerr = 0;
	assign armack = !armwr && armreq || txack;
	
	uart uart0(clk, tx, rx, txreq, txack, txdata, rxreq, rxack, rxdata);

	(* DONT_TOUCH="YES" *) PS7 PS7_i(
		.FCLKCLK(fclkclk),
		
		.MAXIGP0ACLK(clk),
		.MAXIGP0ARVALID(gp_arvalid),
		.MAXIGP0AWVALID(gp_awvalid),
		.MAXIGP0BREADY(gp_bready),
		.MAXIGP0RREADY(gp_rready),
		.MAXIGP0WLAST(gp_wlast),
		.MAXIGP0WVALID(gp_wvalid),
		.MAXIGP0ARID(gp_arid),
		.MAXIGP0AWID(gp_awid),
		.MAXIGP0WID(gp_wid),
		.MAXIGP0ARBURST(gp_arburst),
		.MAXIGP0ARLOCK(gp_arlock),
		.MAXIGP0ARSIZE(gp_arsize),
		.MAXIGP0AWBURST(gp_awburst),
		.MAXIGP0AWLOCK(gp_awlock),
		.MAXIGP0AWSIZE(gp_awsize),
		.MAXIGP0ARPROT(gp_arprot),
		.MAXIGP0AWPROT(gp_awprot),
		.MAXIGP0ARADDR(gp_araddr),
		.MAXIGP0AWADDR(gp_awaddr),
		.MAXIGP0WDATA(gp_wdata),
		.MAXIGP0ARCACHE(gp_arcache),
		.MAXIGP0ARLEN(gp_arlen),
		.MAXIGP0ARQOS(gp_arqos),
		.MAXIGP0AWCACHE(gp_awcache),
		.MAXIGP0AWLEN(gp_awlen),
		.MAXIGP0AWQOS(gp_awqos),
		.MAXIGP0WSTRB(gp_wstrb),
		.MAXIGP0ARREADY(gp_arready),
		.MAXIGP0AWREADY(gp_awready),
		.MAXIGP0BVALID(gp_bvalid),
		.MAXIGP0RLAST(gp_rlast),
		.MAXIGP0RVALID(gp_rvalid),
		.MAXIGP0WREADY(gp_wready),
		.MAXIGP0BID(gp_bid),
		.MAXIGP0RID(gp_rid),
		.MAXIGP0BRESP(gp_bresp),
		.MAXIGP0RRESP(gp_rresp),
		.MAXIGP0RDATA(gp_rdata)

	);

endmodule
