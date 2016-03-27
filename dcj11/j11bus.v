`default_nettype none

module j11bus(
	input wire clk,

	input wire busreq,
	input wire buswr,
	input wire busgp,
	input wire busirq,
	input wire [21:0] busaddr,
	input wire [15:0] buswdata,
	input wire [1:0] buswstrb,
	output reg busack,
	output reg buserr,
	output reg [15:0] busrdata,
	
	output reg busrst,
	
	output reg memreq,
	output wire memwr,
	output wire [21:0] memaddr,
	output wire [15:0] memwdata,
	output wire [1:0] memwstrb,
	input wire memack,
	input wire [15:0] memrdata,
	input wire memerr,
	
	input wire [1:0] uartirq,
	input wire rlirq,
	
	output reg j11init,
	output reg j11halt,
	output reg j11parity,
	output reg j11event,
	output reg [3:0] j11irq,
	output reg j11pwrf,
	output reg j11fpe,
	
	input wire regreq,
	input wire [31:0] regwdata,
	output reg regack,
	
	output wire [5:0] leds
);

	localparam powerup = {1'b0, 2'b01, 1'b1};

	initial begin
		j11init = 1'b0;
		j11halt = 1'b0;
		j11parity = 1'b1;
		j11event = 1'b1;
		j11pwrf = 1'b1;
		j11fpe = 1'b1;
		busrst = 1'b1;
	end
	
	assign memwr = buswr;
	assign memaddr = busaddr;
	assign memwdata = buswdata;
	assign memwstrb = buswstrb;
	
	reg odt;
	assign leds = {4'b0, odt, j11init};
	
	reg [1:0] uartirqact;
	reg rlirqact;
	
	always @(posedge clk) begin
		busack <= 1'b0;
		buserr <= 1'b0;
		memreq <= 1'b0;
		if(busreq)
			if(busgp) begin
				busack <= 1'b1;
				busrdata <= 16'bx;
				case({buswr, busaddr[7:0]})
				{1'b1, 8'o0}: busrdata <= powerup;
				{1'b1, 8'o14}: busrst <= 1'b1;
				{1'b1, 8'o34}: odt <= 1'b1;
				{1'b1, 8'o214}: busrst <= 1'b0;
				{1'b1, 8'o234}: odt <= 1'b0;
				endcase
			end else if(busirq) begin
				busack <= 1'b1;
				busrdata <= 16'bx;
				case(busaddr[3:0])
				4'b0001:
					if(uartirqact[0]) begin
						busrdata <= 16'o60;
						uartirqact[0] <= 1'b0;
					end else if(uartirqact[1]) begin
						busrdata <= 16'o64;
						uartirqact[1] <= 1'b0;
					end
				4'b0010:
					if(rlirq) begin
						busrdata <= 16'o160;
						rlirqact <= 1'b0;
					end
				endcase
			end else
				memreq <= 1'b1;
		if(memack) begin
			busack <= 1'b1;
			busrdata <= memrdata;
			buserr <= memerr;
		end
		if(uartirq[0]) uartirqact[0] <= 1'b1;
		if(uartirq[1]) uartirqact[1] <= 1'b1;
		if(rlirq) rlirqact <= 1'b1;
	end
	
	always @(posedge clk) begin
		j11irq <= 4'b0000;
		if(uartirqact != 0) j11irq[0] <= 1'b1;
		if(rlirqact) j11irq[1] <= 1'b1;
	end
	
	always @(posedge clk) begin
		regack <= regreq;
		if(regreq)
			j11init <= regwdata[0];
	end

endmodule
