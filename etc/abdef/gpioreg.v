`default_nettype none

module gpioreg(
	input wire clk,
	
	input wire regreq,
	output reg regack,
	output reg regerr,
	input wire [5:0] regaddr,
	input wire regwr,
	input wire [31:0] regwdata,
	output reg [31:0] regrdata,
	
	inout wire [29:0] gpio
);

	genvar i;

	wire [29:0] gpio_in;
	reg [29:0] gpio_out = 0;
	reg [29:0] gpio_tri = -1;
	for(i = 0; i < 30; i = i + 1) begin
		assign gpio[i] = gpio_tri[i] ? 1'bz : gpio_out[i];
		assign gpio_in[i] = gpio[i];
	end
	
	always @(posedge clk) begin
		regack <= 1'b0;
		if(regreq) begin
			regack <= 1'b1;
			regerr <= 1'b0;
			case({regaddr & ~3, regwr})
			0: regrdata <= gpio_in;
			'b01_0000_0: regrdata <= gpio_out;
			'b01_0000_1: gpio_out <= regwdata;
			'b01_0100_0: regrdata <= 0;
			'b01_0100_1: gpio_out <= gpio_out | regwdata;
			'b01_1000_0: regrdata <= 0;
			'b01_1000_1: gpio_out <= gpio_out & ~regwdata;
			'b10_0000_0: regrdata <= gpio_tri;
			'b10_0000_1: gpio_tri <= regwdata;
			'b10_0100_0: regrdata <= 0;
			'b10_0100_1: gpio_tri <= gpio_tri | regwdata;
			'b10_1000_0: regrdata <= 0;
			'b10_1000_1: gpio_tri <= gpio_tri & ~regwdata;
			default: regerr <= 1'b1;
			endcase
		end
	end

endmodule
