`include "dat.vh"

module debug #(parameter N = 128, parameter MEM = 1024) (
	input wire clk,
	input wire trigger,
	input wire [N-1:0] indata

`ifdef SIMULATION
	,
	
	input wire capture,
	input wire shift,
	input wire drck,
	input wire reset,
	input wire tdi,
	output reg tdo
);
`else
	);

	wire capture, shift, drck, reset, tdi;
	reg tdo;
	BSCANE2 #(.JTAG_CHAIN(1)) bscane2(
		.CAPTURE(capture),
		.DRCK(drck),
		.RESET(reset),
		.SHIFT(shift),
		.TDI(tdi),
		.TDO(tdo)
	);
`endif

	localparam MAXSADDR = (N + 15) / 16 - 1;
	localparam NR = N + 15 & -16;

	reg [N-1:0] indata0, indata1;
	reg trigger0;
	always @(posedge clk) begin
		indata0 <= indata;
		indata1 <= indata0;
		trigger0 <= trigger;
	end
	
	wire arm;
	reg arm0;
	reg armed, running;
	reg [N-1:0] mem[0:MEM - 1];
	reg [15:0] ptr;
	
	initial begin
		armed = 1;
		running = 0;
	end
	
	always @(posedge clk) begin
		arm0 <= arm;
		if(running) begin
			mem[ptr] <= indata1;
			ptr <= ptr + 1;
			if(ptr == MEM - 1)
				running <= 0;
		end else if(trigger0 && armed) begin
			ptr <= 0;
			armed <= 0;
			running <= 1;
		end else if(arm ^ arm0) begin
			armed <= 1;
		end
	end

	reg [15:0] addr, saddr, addr_, saddr_;
	reg [15:0] data, rddata, data_;
	reg state;
	localparam ADDR = 0;
	localparam DATA = 1;
	reg [3:0] ctr;
	reg [N-1:0] memdata;
	wire [NR-1:0] memdataz = {memdata, {NR-N{1'b0}}};
	
	wire we = shift && state == DATA && ctr == 15;
	
	always @(*) begin
		if(state == ADDR) begin
			addr_ = {addr[14:0], tdi};
			saddr_ = 0;
		end else if(saddr == MAXSADDR) begin
			addr_ = addr + 1;
			saddr_ = 0;
		end else begin
			addr_ = addr;
			saddr_ = saddr + 1;
		end
		if(ctr == 0 && saddr == 0 && addr[15:8] != 8'hff) begin
			data_ = {memdataz[NR - 2 -: 15], tdi};
			tdo = memdata[N - 1];
		end else begin
			data_ = {data[14:0], tdi};
			tdo = data[15];
		end
	end

	always @(posedge drck) begin
		if(capture) begin
			state <= ADDR;
			saddr <= 0;
			data <= 0;
			ctr <= 0;
		end
		if(shift)
			case(state)
			ADDR: begin
				addr <= addr_;
				saddr <= saddr_;
				ctr <= ctr + 1;
				if(ctr == 15) begin
					state <= DATA;
					data <= rddata;
				end
			end
			DATA: begin
				data <= data_;
				ctr <= ctr + 1;
				if(ctr == 15) begin
					saddr <= saddr_;
					addr <= addr_;
					data <= rddata;
				end
			end
			endcase
	end
	
	wire jarmed, jrunning;
	reg jarm;
	initial jarm = 0;
	sync armedsync(drck, armed, jarmed);
	sync runningsync(drck, running, jrunning);
	sync armsync(clk, jarm, arm);
	
	always @(*) begin
		rddata = 16'bx;
		if(addr_[15:8] == 8'hff)
			casez(addr_[7:0])
			0: rddata = {14'b0, jarmed, jrunning};
			2: rddata = N;
			3: rddata = MEM;
			endcase
		else 
			rddata = memdataz[NR - 1 - saddr_ * 16 -: 16];
	end
	
	always @(posedge drck)
		memdata <= mem[addr_];
	
	always @(posedge drck)
		if(we && addr[15:8] == 8'hff)
			case(addr[7:0])
			1: jarm <= !jarm;
			endcase

endmodule
