`default_nettype none

module j11bus(
	input wire clk,

	input wire busreq,
	input wire buswr,
	input wire busgp,
	input wire [21:0] busaddr,
	input wire [15:0] buswdata,
	output reg busack,
	output reg buserr,
	output reg [15:0] busrdata,
	
	output reg busrst,
	
	output reg memreq,
	output wire memwr,
	output wire [21:0] memaddr,
	output wire [15:0] memwdata,
	input wire memack,
	input wire [15:0] memrdata,
	
	output reg uartreq,
	output wire [2:0] uartaddr,
	output wire uartwr,
	output wire [15:0] uartwdata,
	input wire uartack,
	input wire [15:0] uartrdata,
	
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
		j11irq = 4'b1111;
		j11pwrf = 1'b1;
		j11fpe = 1'b1;
		busrst = 1'b1;
	end
	
	assign memwr = buswr;
	assign memaddr = busaddr;
	assign memwdata = buswdata;
	
	assign uartwr = buswr;
	assign uartaddr = busaddr;
	assign uartwdata = buswdata;
	
	reg odt;
	assign leds = {4'b0, odt, j11init};
	
	always @(posedge clk) begin
		busack <= 1'b0;
		buserr <= 1'b0;
		memreq <= 1'b0;
		uartreq <= 1'b0;
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
			end else begin
				if(busaddr < 22'o17760000)
					memreq <= 1'b1;
				else
					casex(busaddr[12:0])
					13'o1756x: uartreq <= 1'b1;
					default: begin
						busack <= 1'b1;
						buserr <= 1'b1;
					end
					endcase
					
			end
		if(memack) begin
			busack <= 1'b1;
			busrdata <= memrdata;
		end
		if(uartack) begin
			busack <= 1'b1;
			busrdata <= uartrdata;
		end
	end
	
	
	always @(posedge clk) begin
		regack <= regreq;
		if(regreq)
			j11init <= regwdata[0];
	end

endmodule
