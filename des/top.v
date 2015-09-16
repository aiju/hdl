`default_nettype none

module top(
);

	localparam N = 16;
	wire [3:0] fclk;
	wire clk = fclk[0];
	wire fastclk = fclk[1];
	wire [3:0] fresetn;
	wire gp0_resetn = fresetn[0];
	
	/*AUTOWIRE*/
	// Beginning of automatic wires (for undeclared instantiated-module outputs)
	wire		armack;			// From regs of regs.v
	wire [31:0]	armaddr;		// From axi3 of axi3.v
	wire		armerr;			// From regs of regs.v
	wire [31:0]	armrdata;		// From regs of regs.v
	wire		armreq;			// From axi3 of axi3.v
	wire [31:0]	armwdata;		// From axi3 of axi3.v
	wire		armwr;			// From axi3 of axi3.v
	wire [(32)/8-1:0] armwstrb;		// From axi3 of axi3.v
	wire [63:0]	goal;			// From regs of regs.v
	wire [N-1:0]	run;			// From regs of regs.v
	wire [63:0]	start;			// From regs of regs.v
	// End of automatics

	wire gp0_awvalid;
	wire gp0_awready;
	wire [1:0] gp0_awburst, gp0_awlock;
	wire [2:0] gp0_awsize, gp0_awprot;
	wire [3:0] gp0_awlen, gp0_awcache, gp0_awqos;
	wire [11:0] gp0_awid;
	wire [31:0] gp0_awaddr;

	wire gp0_arvalid;
	wire gp0_arready;
	wire [1:0] gp0_arburst, gp0_arlock;
	wire [2:0] gp0_arsize, gp0_arprot;
	wire [3:0] gp0_arlen, gp0_arcache, gp0_arqos;
	wire [11:0] gp0_arid;
	wire [31:0] gp0_araddr;

	wire gp0_wvalid, gp0_wlast;
	wire gp0_wready;
	wire [3:0] gp0_wstrb;
	wire [11:0] gp0_wid;
	wire [31:0] gp0_wdata;

	wire gp0_bvalid;
	wire gp0_bready;
	wire [1:0] gp0_bresp;
	wire [11:0] gp0_bid;

	wire gp0_rvalid;
	wire gp0_rready;
	wire [1:0] gp0_rresp;
	wire gp0_rlast;
	wire [11:0] gp0_rid;
	wire [31:0] gp0_rdata;

	wire [N-1:0] busy;
	wire [64*N-1:0] res;
	
	/* axi3 AUTO_TEMPLATE (
		.\(\(aw\|ar\|w\|b\|r\).*\)(gp0_\1[]),
		.out\(.*\)(arm\1[]),
	); */
	axi3 #(.ADDR(32), .DATA(32), .ID(12)) axi3(/*AUTOINST*/
						   // Outputs
						   .awready		(gp0_awready),	 // Templated
						   .arready		(gp0_arready),	 // Templated
						   .wready		(gp0_wready),	 // Templated
						   .bvalid		(gp0_bvalid),	 // Templated
						   .bresp		(gp0_bresp[1:0]), // Templated
						   .bid			(gp0_bid[11:0]), // Templated
						   .rvalid		(gp0_rvalid),	 // Templated
						   .rresp		(gp0_rresp[1:0]), // Templated
						   .rlast		(gp0_rlast),	 // Templated
						   .rid			(gp0_rid[11:0]), // Templated
						   .rdata		(gp0_rdata[31:0]), // Templated
						   .outaddr		(armaddr[31:0]), // Templated
						   .outwdata		(armwdata[31:0]), // Templated
						   .outwr		(armwr),	 // Templated
						   .outreq		(armreq),	 // Templated
						   .outwstrb		(armwstrb[(32)/8-1:0]), // Templated
						   // Inputs
						   .clk			(clk),
						   .resetn		(gp0_resetn),	 // Templated
						   .awvalid		(gp0_awvalid),	 // Templated
						   .awburst		(gp0_awburst[1:0]), // Templated
						   .awlock		(gp0_awlock[1:0]), // Templated
						   .awsize		(gp0_awsize[2:0]), // Templated
						   .awprot		(gp0_awprot[2:0]), // Templated
						   .awlen		(gp0_awlen[3:0]), // Templated
						   .awcache		(gp0_awcache[3:0]), // Templated
						   .awqos		(gp0_awqos[3:0]), // Templated
						   .awid		(gp0_awid[11:0]), // Templated
						   .awaddr		(gp0_awaddr[31:0]), // Templated
						   .arvalid		(gp0_arvalid),	 // Templated
						   .arburst		(gp0_arburst[1:0]), // Templated
						   .arlock		(gp0_arlock[1:0]), // Templated
						   .arsize		(gp0_arsize[2:0]), // Templated
						   .arprot		(gp0_arprot[2:0]), // Templated
						   .arlen		(gp0_arlen[3:0]), // Templated
						   .arcache		(gp0_arcache[3:0]), // Templated
						   .arqos		(gp0_arqos[3:0]), // Templated
						   .arid		(gp0_arid[11:0]), // Templated
						   .araddr		(gp0_araddr[31:0]), // Templated
						   .wvalid		(gp0_wvalid),	 // Templated
						   .wlast		(gp0_wlast),	 // Templated
						   .wstrb		(gp0_wstrb[(32)/8-1:0]), // Templated
						   .wid			(gp0_wid[11:0]), // Templated
						   .wdata		(gp0_wdata[31:0]), // Templated
						   .bready		(gp0_bready),	 // Templated
						   .rready		(gp0_rready),	 // Templated
						   .outrdata		(armrdata[31:0]), // Templated
						   .outack		(armack),	 // Templated
						   .outerr		(armerr));	 // Templated
	
	regs #(/*AUTOINSTPARAM*/
	       // Parameters
	       .N			(N)) regs(/*AUTOINST*/
						  // Outputs
						  .armrdata		(armrdata[31:0]),
						  .armack		(armack),
						  .armerr		(armerr),
						  .start		(start[63:0]),
						  .goal			(goal[63:0]),
						  .run			(run[N-1:0]),
						  // Inputs
						  .clk			(clk),
						  .armaddr		(armaddr[31:0]),
						  .armwdata		(armwdata[31:0]),
						  .armwr		(armwr),
						  .armreq		(armreq),
						  .armwstrb		(armwstrb[3:0]),
						  .busy			(busy[N-1:0]),
						  .res			(res[64*N-1:0]));
	
	genvar i;
	generate
		for(i = 0; i < N; i = i + 1) begin
			descrack descrack(
				.clk(fastclk),
				.run(run[i]),
				.busy(busy[i]),
				.result(res[i * 64 +: 64]),
				/*AUTOINST*/
					  // Inputs
					  .start		(start[63:0]),
					  .goal			(goal[63:0]));
		end
	endgenerate
	
	(*DONT_TOUCH = "TRUE"*) PS7 PS7(
		.MAXIGP0ARVALID(gp0_arvalid),
		.MAXIGP0AWVALID(gp0_awvalid),
		.MAXIGP0BREADY(gp0_bready),
		.MAXIGP0RREADY(gp0_rready),
		.MAXIGP0WLAST(gp0_wlast),
		.MAXIGP0WVALID(gp0_wvalid),
		.MAXIGP0ARID(gp0_arid),
		.MAXIGP0AWID(gp0_awid),
		.MAXIGP0WID(gp0_wid),
		.MAXIGP0ARBURST(gp0_arburst),
		.MAXIGP0ARLOCK(gp0_arlock),
		.MAXIGP0ARSIZE(gp0_arsize),
		.MAXIGP0AWBURST(gp0_awburst),
		.MAXIGP0AWLOCK(gp0_awlock),
		.MAXIGP0AWSIZE(gp0_awsize),
		.MAXIGP0ARPROT(gp0_arprot),
		.MAXIGP0AWPROT(gp0_awprot),
		.MAXIGP0ARADDR(gp0_araddr),
		.MAXIGP0AWADDR(gp0_awaddr),
		.MAXIGP0WDATA(gp0_wdata),
		.MAXIGP0ARCACHE(gp0_arcache),
		.MAXIGP0ARLEN(gp0_arlen),
		.MAXIGP0ARQOS(gp0_arqos),
		.MAXIGP0AWCACHE(gp0_awcache),
		.MAXIGP0AWLEN(gp0_awlen),
		.MAXIGP0AWQOS(gp0_awqos),
		.MAXIGP0WSTRB(gp0_wstrb),
		.MAXIGP0ACLK(clk),
		.MAXIGP0ARREADY(gp0_arready),
		.MAXIGP0AWREADY(gp0_awready),
		.MAXIGP0BVALID(gp0_bvalid),
		.MAXIGP0RLAST(gp0_rlast),
		.MAXIGP0RVALID(gp0_rvalid),
		.MAXIGP0WREADY(gp0_wready),
		.MAXIGP0BID(gp0_bid),
		.MAXIGP0RID(gp0_rid),
		.MAXIGP0BRESP(gp0_bresp),
		.MAXIGP0RRESP(gp0_rresp),
		.MAXIGP0RDATA(gp0_rdata),
		.FCLKCLK(fclk),
		.FCLKRESETN(fresetn)
	);

endmodule

// Local Variables:
// verilog-auto-inst-param-value: t
// End:
